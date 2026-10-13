# Order Book / Matching Engine -- Learning Project

A build-it-yourself limit-order-book matching engine in modern C++, structured as a graded
set of milestones (M1 -> M4). It exercises exactly the skills a **quant developer** interview
probes: correct systems code, cache-aware data structures, lock-free concurrency, and latency
benchmarking. This single file is the complete instructions: concept, requirements, how to
build/run, and the phase-by-phase guide.

---

## 0. Why this project

- A concrete, deployable portfolio piece to link from your CV and talk through in interviews.
- It forces the "production C++" skills that competitive-programming C++ does not: ownership
  (RAII, rule of 5), cache/data-oriented layout, `std::atomic` / memory ordering, and
  measurement-driven optimization.
- You implement the **skeleton**; a hidden **reference** is the answer key; a shared
  **test-suite** grades you. Correctness is checked three ways: scripted unit tests, a
  **differential test** (random command streams compared against an independent
  "obviously-correct" oracle after every step), and **sanitizers** (ASan / UBSan / TSan).

---

## 1. What is an order book / a matching engine?

### Order book
For a single instrument (e.g. one stock), the exchange keeps every **resting** (waiting) order,
split into two sides and sorted by price:

- **Bids** = buy orders, sorted **high -> low**; the highest price is the *best bid*.
- **Asks** (offers) = sell orders, sorted **low -> high**; the lowest price is the *best ask*.
- **Spread** = best ask - best bid.
- A price level can hold many orders; within a level they are kept in arrival order (**FIFO**).

```
        ASKS (sells)     price ascending, best = lowest
          102 x 20
          101 x 8        <- best ask
        ------------       (spread = 101 - 100 = 1)
          100 x 4        <- best bid
           99 x 5
        BIDS (buys)      price descending, best = highest
```

### Matching engine
When a **new order** arrives, the engine tries to **match** it against the opposite side:

- A **buy** at price P matches sells priced **<= P**, cheapest first.
- A **sell** at price P matches buys priced **>= P**, highest first.
- Within the same price, the **oldest** resting order trades first (time priority).
- **Execution price = the resting (maker) order's price** (price improvement goes to the taker).
- If the incoming order is not fully filled: a **LIMIT** order's remainder **rests** in the book;
  a **MARKET** order's remainder is **discarded**.
- Every match emits a **Trade** (execution).

### Glossary
| Term | Meaning |
|---|---|
| Limit order | Has a price bound: buy <= X or sell >= X. Matches what it can; the rest rests. |
| Market order | No price bound: matches at any available price until filled or the book is empty; remainder discarded. |
| Cancel | Remove a resting order by OrderId. |
| Maker / Taker | Maker = the resting order (provides liquidity); Taker = the incoming order (removes it). |
| Partial fill | An order matched only in part (e.g. order for 10, matched 6, 4 remain). |
| Crossed book | An illegal state where best bid >= best ask. Must never persist after matching. |
| Tick | The smallest price increment. **Prices are integers**, never floating point. |

---

## 2. Worked example (read this to "get it")

Start with an empty book. Notation: `LIMIT BUY 10@100` = buy 10 units at price 100.

| # | Incoming | What happens | Book after | Trades emitted |
|---|---|---|---|---|
| 1 | `LIMIT BUY 10@100`  | no asks to match -> rests                       | Bid: 100x10                | -- |
| 2 | `LIMIT BUY 5@99`    | rests                                           | Bid: 100x10, 99x5          | -- |
| 3 | `LIMIT SELL 8@101`  | best bid 100 < 101 -> no cross -> rests          | Bid unchanged / Ask: 101x8 | -- |
| 4 | `LIMIT SELL 6@100`  | 100 <= best bid 100 -> trade 6 with bid 100      | Bid: 100x**4**, 99x5       | 6 @ 100 |
| 5 | `LIMIT SELL 7@99`   | takes bid 100x4 (best), then bid 99x3           | Bid: 99x**2** / Ask: 101x8 | 4 @ 100; 3 @ 99 |
| 6 | `MARKET BUY 10`     | eats ask 101x8, 2 left, no more asks -> discarded | Bid: 99x2 / Ask: empty     | 8 @ 101 |

Note step 5: the sell "sweeps" two price levels in best-first order, and each trade prints at the
**maker's** price (100 then 99), not the incoming order's 99.

---

## 3. Requirements

**Functional**
- **FR1** Order types: `LIMIT` (buy/sell), `MARKET` (buy/sell), `CANCEL`.
- **FR2** Matching: price-time priority; execution at the maker's price.
- **FR3** Partial fills on both sides, with correct remaining-quantity accounting.
- **FR4** Market order sweeps until filled or the book empties; remainder discarded (not rested).
- **FR5** Cancel by OrderId; returns false if not found.
- **FR6** Each operation returns the list of `Trade`s it produced.
- **FR7** Queries: best bid, best ask, total quantity at a given price (for tests + market data).
- **FR8** Invariants that always hold after an operation: the book is never crossed
  (best bid < best ask), total quantity is conserved (a match subtracts equally from both sides),
  no negative quantities.
- **Precondition:** an OrderId is unique among currently-live orders (real exchanges guarantee
  this; `rest_()` asserts it).

**Edge cases to handle:** market order on an empty book; cancel of an absent/already-filled id;
incoming quantity exactly equal to a resting order; a taker that sweeps several levels; two orders
at the same price/time (keep insertion order); zero/negative quantity (reject).

**Non-functional**
- **NFR1 Correctness:** pass the suite (Section 7), clean under ASan/UBSan.
- **NFR2 Complexity:** best bid/ask **O(1)**; add **O(log P)**; cancel **O(log P)** for the map
  (the price-level lookup); with a price-ladder array cancel becomes **O(1)**.
- **NFR3 Hot path (from M2):** no dynamic allocation **inside the book** (use an object pool),
  no lock, no throw, no syscall on the matching path. (The one unavoidable allocation is the
  returned `std::vector<Trade>`; a truly alloc-free path needs a caller-supplied output span or
  callback, which would change the frozen API -- a good interview talking point.)
- **NFR4 Latency (M4):** measurable p50/p99/p99.9 per operation; reproducible benchmark.
- **NFR5 Thread-safety (M3):** clean under ThreadSanitizer.

---

## 4. Public API (the stable contract)

The public interface is frozen: M2 keeps it identical and only swaps the internals, so the same
tests validate both engines.

```cpp
using OrderId = std::uint64_t;
using Price   = std::int64_t;   // integer ticks -- never a double
using Qty     = std::uint64_t;
enum class Side { Buy, Sell };

struct Trade { OrderId taker; OrderId maker; Price price; Qty qty; }; // price == maker's price

class MatchingEngine {                       // non-copyable, movable (owns internal iterators)
public:
    std::vector<Trade> add_limit (OrderId id, Side side, Price price, Qty qty); // remainder rests
    std::vector<Trade> add_market(OrderId id, Side side, Qty qty);              // remainder discarded
    bool               cancel    (OrderId id);                                  // false if absent
    std::optional<Price> best_bid() const noexcept;
    std::optional<Price> best_ask() const noexcept;
    Qty                  qty_at  (Side side, Price price) const noexcept;
    bool                 empty   () const noexcept;
};
```

Interview-relevant choices: integer tick prices (no float error); an id->location map for O(1)
lookup; the engine is `= delete`d-copy / `= default`-move because it stores iterators/pointers
into its own containers (rule of 5).

---

## 5. CLI command format

`./build/cli` reads commands from stdin, prints trades and top-of-book:

```
L B|S <id> <qty> <px>    limit order      e.g.  L B 1 10 100
M B|S <id> <qty>         market order     e.g.  M S 2 5
C <id>                   cancel
P                        print top-of-book
Q                        quit
```

---

## 6. Repository layout

```
orderbook/
|- README.md                     this file (all instructions)
|- include/ob/
|  |- types.hpp                  OrderId/Price/Qty/Side/Trade         (given, complete)
|  |- testkit.hpp                tiny test framework + oracle engine  (given, complete)
|- skeleton/ob/                  <-- YOU IMPLEMENT THESE
|  |- matching_engine.hpp        (M1)
|  |- matching_engine_fast.hpp   (M2)
|  |- spsc_queue.hpp             (M3)
|- reference/ob/                 answer key -- peek only after trying
|  |- matching_engine.hpp        (M1)
|  |- matching_engine_fast.hpp   (M2)
|  |- spsc_queue.hpp             (M3)
|- tests/
|  |- engine_tests.hpp           shared engine test-suite (M1 & M2)
|  |- test_engine.cpp            M1 driver
|  |- test_engine_m2.cpp         M2 driver
|  |- test_spsc.cpp              M3 driver
|- bench/benchmark.cpp           M4 latency harness
|- tools/cli.cpp                 interactive REPL
|- build.sh                      builds + runs everything (authoritative)
|- CMakeLists.txt                reference-target build via CMake
```

---

## 7. How grading works

- **Scripted tests** replay the Section-2 worked example plus targeted edge cases.
- **Differential test:** 30k random commands are fed to your engine AND to `BruteMatchingEngine`
  (an independent O(n) linear-scan oracle in `testkit.hpp`); after every command the full book
  state (via the public API) and the trade stream must match. Two different implementations
  agreeing on 30k random steps is strong evidence of correctness.
- **Sanitizers:** ASan/UBSan on every test; TSan on the concurrency tests. On arm64 (a weak
  memory model) a wrong `memory_order` is a real bug that a plain run may miss, so TSan is the
  real net for M3.

---

## 8. Quickstart

```bash
./build.sh                 # AUTHORITATIVE: builds+runs every target with ASan/UBSan/TSan and
                           # grades your skeleton (reference passes; skeleton fails until done).
# CMake is a convenience that builds the reference targets + benchmark + CLI only
# (no sanitizers, no skeleton grading -- use build.sh for those):
cmake -S . -B build && cmake --build build -j && ctest --test-dir build
```

Play by hand:
```bash
./build/cli
> L S 1 5 101      # sell 5 @ 101
> L B 2 3 101      # buy 3 @ 101 -> trade
> P                # print top of book
```

**The loop you follow each phase:** read the phase below -> implement the TODOs in the `skeleton/`
file -> `./build.sh` turns your `*_skel` target from red to green -> diff your file against
`reference/` to compare approach -> move on. **Correct first, fast second.**

---

## 9. Phase guide (M1 -> M4)

### M1 -- A correct single-threaded engine
**Goal:** implement price-time-priority matching correctly. Everything else builds on this.
**Build (fill `skeleton/ob/matching_engine.hpp`):** `add_limit`, `add_market`, `cancel`, and the
private `match_` (core loop) and `rest_`.
**Rules:** best price first; oldest-first within a price (FIFO); trade at the maker's price; a BUY
crosses asks priced <= its limit, a SELL crosses bids priced >= its limit; assert the unique-id
precondition in `rest_`.
**Verify:** `./build.sh` -> `test_engine_skel` prints `failed=0` (worked example + edges + 30k-step
differential), clean under ASan/UBSan.
**Skills:** RAII, class design, STL (`map`/`list`/`unordered_map`), ownership, rule of 0/5
(non-copyable, movable), iterator validity.

### M2 -- Optimize the data structures
**Goal:** identical public behaviour, faster hot path, allocation-free **inside the book** (the
returned `vector<Trade>` still allocates -- see NFR3).
**Build (fill `skeleton/ob/matching_engine_fast.hpp`, class `MatchingEngineFast`):**
- Intrusive doubly-linked list per price level -> O(1) unlink given the node. (`std::list::erase`
  was already O(1); the real win here is **zero per-order allocation**. cancel() stays O(log P) for
  the price-level lookup until the ladder stretch.)
- Object pool (a `std::deque` for stable addresses + a free list) -> no `new`/`delete` on the hot path.
- (Stretch) replace `std::map` with a **price-ladder array** indexed by tick -> then best-quote AND
  cancel are O(1) and cache-friendly.
**Verify:** `./build.sh` grades your file as `test_engine_m2_skel` with the SAME suite as M1. Then
compare `./build/benchmark` (M1) against an adapted copy for M2.
**Skills:** cache/data-oriented design, intrusive containers, pool allocators, stable-API refactoring.

### M3 -- Lock-free SPSC queue (concurrency)
**Goal:** decouple a producer (feed/parse) thread from the engine (consumer) thread with a bounded
lock-free ring buffer -- the standard shape of a trading data path.
**Build (fill `skeleton/ob/spsc_queue.hpp`):** just `push`/`pop` on a power-of-two ring, using one
reserved slot to tell full from empty, with the correct **acquire/release** memory ordering. (The
cache-line padding -- `alignas(kLine=128)` on `head_`/`tail_`/`buf_`, sized for the arm64 128-byte
line -- is already given; understand WHY it prevents false sharing, but you do not write it.)
**Verify:** `test_spsc_skel` passes single-thread + threaded (1M items in order) + integration; and
`test_spsc_skel_tsan` (your queue under ThreadSanitizer) is clean -- that TSan target is what
actually grades your ordering.
**Skills:** `std::atomic`, `memory_order` (relaxed/acquire/release), the ABA idea, false sharing.

### M4 -- Latency benchmark & tuning
**Goal:** measure, then optimize with evidence.
**Build/use:** `bench/benchmark.cpp` generates a realistic order flow and reports p50/p90/p99/p99.9/max
latency per operation. Adapt it to your M2 engine and compare. Profile (macOS Instruments, or `perf`
on Linux) and remove hot-path costs.
**Verify:** a before/after latency table plus a short note on what each change bought.
**Skills:** benchmarking rigor (warm-up, percentiles not just mean), profiling, low-latency discipline
(no lock / no throw / no syscall; no alloc in the book internals).

---

## 10. Testing strategy (do this alongside, not at the end)
- Unit tests: the worked example + each edge case.
- Invariant/property checks: after any random sequence, the book is never crossed, quantity is
  conserved, nothing is negative.
- Differential testing: compare against the independent brute-force oracle on random input.
- Sanitizers: ASan/UBSan always; TSan for M3.
- Benchmark: warm up, then report percentiles (not just the mean).

## 11. Design rules (do not skip)
- **Price is an integer tick** (`std::int64_t`), never a float.
- **Public API is a stable contract** -- M2 changes only internals; the same tests must pass.
- **Trade price = the resting (maker) order's price.**
- The engine owns internal self-references, so it is **non-copyable** (rule of 5).

## 12. Stretch goals
IOC/FOK order types; multiple instruments; top-of-book / L2 market-data output; a binary wire
protocol + replay-from-file; an MPSC queue for multiple feeds; self-trade prevention; a
`std::map` vs price-ladder A/B benchmark.

## 13. Interview talking points this project earns you
- value categories & when a move happens; rule of 0/3/5 (why the engine deletes copy);
  `unique_ptr` vs `shared_ptr` cost.
- why `std::map`/`std::unordered_map` are avoided on an HFT hot path; intrusive containers; object pools.
- `memory_order` and why acquire/release suffices for an SPSC queue; false sharing (128-byte line on
  Apple Silicon).
- big-O: add O(log P), cancel O(log P) (map) -> O(1) with a price-ladder, best-quote O(1).
- how you verified correctness (differential testing vs an independent oracle + sanitizers); how you
  measured latency (percentiles, warm-up) and what you optimized.
