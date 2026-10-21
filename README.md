# C++20 Quant & Systems Interview Practice Repository

Prioritized, hands-on C++20 systems & quantitative engineering practice repository covering all 14 high-frequency trading and Big Tech core infrastructure problems.

## Quick Start (CMake + Presets)

```bash
# 1. Configure & test under AddressSanitizer + UndefinedBehaviorSanitizer (Tasks #1, #3, #4, #5, #7, #8, #13)
cmake --preset debug
cmake --build build/debug -j
ctest --preset debug -R 01_limit_order_book -V

# 2. Configure & test under ThreadSanitizer (Tasks #2, #3, #6, #9, #10, #11, #14)
cmake --preset tsan
cmake --build build/tsan -j
ctest --preset tsan -R 02_spsc_ring_buffer -V

# 3. Benchmark mode with -O3 (Tasks #2, #9, #12, #14)
cmake --preset release
cmake --build build/release -j
./build/release/test_12_memory_layout_and_false_sharing
```

## Quick Start (Fast Single-Command Makefile)

```bash
make TASK=01 SAN=asan     # Limit Order Book with ASan+UBSan
make TASK=02 SAN=tsan     # Lock-Free SPSC Ring Buffer with ThreadSanitizer
make TASK=12 SAN=bench    # Memory Layout, False Sharing & AoS vs SoA at -O3
```

## Prioritized Study Schedule

| Available Time | Target Tasks | Core Focus |
|---|---|---|
| **2 to 3 hours** | `#01` to `#04` | Order book matching, lock-free SPSC memory orders, smart pointers & custom allocators |
| **5 to 6 hours** | `#01` to `#08` | Adds numerically stable rolling stats, thread pool, `Vector<T>` exception safety & binary parser |
| **8+ hours** | `#01` to `#14` | Adds Black-Scholes/MC, sharded LRU, blocking queue, false sharing/SoA, SBO `Function`, async logger |

## Mandatory Verbal Protocol (Say Out Loud After Every Task)
1. **Complexity**: Exact asymptotic worst-case vs amortized time and space per operation.
2. **Memory Layout**: Object byte footprint, padding, cache-line residency, and pointer indirections.
3. **Concurrency Hazards**: Race conditions, ABA hazards, memory ordering edges, and lock contention.
4. **Low-Latency Upgrades**: Concrete architectural transformations for sub-microsecond hot paths.
