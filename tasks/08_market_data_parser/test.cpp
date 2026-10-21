#include "feed_parser.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <cstring>
#include <tuple>
#include <utility>
#include <vector>

namespace {

class WireBuilder {
public:
    void push_u8(uint8_t v) { bytes_.push_back(v); }
    void push_u16(uint16_t v) {
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    void push_u32(uint32_t v) {
        for (int s = 24; s >= 0; s -= 8) {
            bytes_.push_back(static_cast<uint8_t>((v >> s) & 0xFF));
        }
    }
    void push_u64(uint64_t v) {
        for (int s = 56; s >= 0; s -= 8) {
            bytes_.push_back(static_cast<uint8_t>((v >> s) & 0xFF));
        }
    }

    void add_msg(uint64_t seq, uint64_t id, char side, uint32_t qty, int64_t px) {
        push_u16(32);
        push_u8(static_cast<uint8_t>('A'));
        push_u64(seq);
        push_u64(id);
        push_u8(static_cast<uint8_t>(side));
        push_u32(qty);
        push_u64(static_cast<uint64_t>(px));
    }

    void cancel_msg(uint64_t seq, uint64_t id) {
        push_u16(19);
        push_u8(static_cast<uint8_t>('X'));
        push_u64(seq);
        push_u64(id);
    }

    void exec_msg(uint64_t seq, uint64_t id, uint32_t qty) {
        push_u16(23);
        push_u8(static_cast<uint8_t>('E'));
        push_u64(seq);
        push_u64(id);
        push_u32(qty);
    }

    const std::vector<uint8_t>& data() const { return bytes_; }

private:
    std::vector<uint8_t> bytes_;
};

}  // namespace

TEST_CASE("01. Decodes Add, Cancel, and Execute frames including negative spread ticks") {
    WireBuilder wb;
    wb.add_msg(0, 0x0102030405060708ULL, 'B', 1500, 99500);
    wb.add_msg(1, 0xAABBCCDDEEFF0011ULL, 'S', 250, -425);  // Calendar spread negative price
    wb.exec_msg(2, 0x0102030405060708ULL, 500);
    wb.cancel_msg(3, 0xAABBCCDDEEFF0011ULL);

    std::vector<std::tuple<uint64_t, char, uint32_t, int64_t>> adds;
    std::vector<std::pair<uint64_t, uint32_t>> execs;
    std::vector<uint64_t> cancels;
    std::vector<std::pair<uint64_t, uint64_t>> gaps;

    Handler h{
        [&](uint64_t id, char s, uint32_t q, int64_t p) { adds.emplace_back(id, s, q, p); },
        [&](uint64_t id) { cancels.push_back(id); },
        [&](uint64_t id, uint32_t q) { execs.emplace_back(id, q); },
        [&](uint64_t exp, uint64_t got) { gaps.emplace_back(exp, got); },
    };

    FeedParser parser;
    const size_t consumed = parser.parse(wb.data().data(), wb.data().size(), h);
    REQUIRE_EQ(consumed, wb.data().size());
    REQUIRE_EQ(parser.next_expected_seq(), uint64_t{4});
    REQUIRE(gaps.empty());

    REQUIRE_EQ(adds.size(), size_t{2});
    REQUIRE(adds[0] == std::make_tuple(uint64_t{0x0102030405060708ULL}, 'B', uint32_t{1500}, int64_t{99500}));
    REQUIRE(adds[1] == std::make_tuple(uint64_t{0xAABBCCDDEEFF0011ULL}, 'S', uint32_t{250}, int64_t{-425}));
    REQUIRE_EQ(execs.size(), size_t{1});
    REQUIRE(execs[0] == std::make_pair(uint64_t{0x0102030405060708ULL}, uint32_t{500}));
    REQUIRE_EQ(cancels.size(), size_t{1});
    REQUIRE_EQ(cancels[0], uint64_t{0xAABBCCDDEEFF0011ULL});
}

TEST_CASE("02. Streaming arbitrary partial byte chunks (1-byte drip feed)") {
    WireBuilder wb;
    for (uint64_t s = 0; s < 30; ++s) {
        if (s % 3 == 0) wb.add_msg(s, 1000 + s, 'B', 10, 500 + static_cast<int64_t>(s));
        else if (s % 3 == 1) wb.exec_msg(s, 1000 + s - 1, 5);
        else wb.cancel_msg(s, 1000 + s - 2);
    }

    size_t count = 0;
    size_t gap_count = 0;
    Handler h{
        [&](uint64_t, char, uint32_t, int64_t) { ++count; },
        [&](uint64_t) { ++count; },
        [&](uint64_t, uint32_t) { ++count; },
        [&](uint64_t, uint64_t) { ++gap_count; },
    };

    FeedParser parser;
    std::vector<uint8_t> rx_buf;
    for (uint8_t byte : wb.data()) {
        rx_buf.push_back(byte);
        const size_t used = parser.parse(rx_buf.data(), rx_buf.size(), h);
        rx_buf.erase(rx_buf.begin(), rx_buf.begin() + static_cast<ptrdiff_t>(used));
    }

    REQUIRE(rx_buf.empty());
    REQUIRE_EQ(count, size_t{30});
    REQUIRE_EQ(gap_count, size_t{0});
}

TEST_CASE("03. Unaligned buffer offsets (+1, +3, +5) trigger zero UBSan faults") {
    WireBuilder wb;
    wb.add_msg(0, 99999999999ULL, 'S', 42, 123456789LL);
    wb.exec_msg(1, 99999999999ULL, 12);

    alignas(64) uint8_t storage[256]{};
    for (size_t skew : {size_t{1}, size_t{3}, size_t{5}}) {
        std::memcpy(storage + skew, wb.data().data(), wb.data().size());
        int parsed = 0;
        Handler h{
            [&](uint64_t, char, uint32_t, int64_t) { ++parsed; },
            nullptr,
            [&](uint64_t, uint32_t) { ++parsed; },
            nullptr,
        };
        FeedParser p;
        REQUIRE_EQ(p.parse(storage + skew, wb.data().size(), h), wb.data().size());
        REQUIRE_EQ(parsed, 2);
    }
}

TEST_CASE("04. Sequence gap detection and clean resynchronization") {
    WireBuilder wb;
    wb.cancel_msg(0, 1);
    wb.cancel_msg(1, 2);
    wb.cancel_msg(7, 3);   // Gap: expected 2, got 7
    wb.cancel_msg(8, 4);   // Contiguous after resync!
    wb.cancel_msg(20, 5);  // Gap: expected 9, got 20

    std::vector<std::pair<uint64_t, uint64_t>> gaps;
    Handler h{
        nullptr,
        nullptr,
        nullptr,
        [&](uint64_t exp, uint64_t got) { gaps.emplace_back(exp, got); },
    };

    FeedParser parser;
    REQUIRE_EQ(parser.parse(wb.data().data(), wb.data().size(), h), wb.data().size());
    REQUIRE_EQ(gaps.size(), size_t{2});
    REQUIRE(gaps[0] == std::make_pair(uint64_t{2}, uint64_t{7}));
    REQUIRE(gaps[1] == std::make_pair(uint64_t{9}, uint64_t{20}));
    REQUIRE_EQ(parser.next_expected_seq(), uint64_t{21});
}

TEST_CASE("05. Corrupt <11-byte header length raises exception") {
    WireBuilder wb;
    wb.push_u16(6);  // Invalid header size < 11!
    wb.push_u8('X');
    wb.push_u64(0);

    Handler h{};
    FeedParser parser;
    REQUIRE_THROWS(parser.parse(wb.data().data(), wb.data().size(), h));
}

RUN_TEST_SUITE("08 — Binary Market Data Feed Parser")
