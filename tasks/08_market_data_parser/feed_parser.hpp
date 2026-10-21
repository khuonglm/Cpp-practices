#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>

struct Handler {
    std::function<void(uint64_t id, char side, uint32_t qty, int64_t px)> on_add;
    std::function<void(uint64_t id)>                                      on_cancel;
    std::function<void(uint64_t id, uint32_t qty)>                        on_exec;
    std::function<void(uint64_t expected, uint64_t got)>                  on_gap;
};

class FeedParser {
public:
    FeedParser() = default;

    // Wire specification (all multi-byte fields Big Endian):
    //   Header (11 bytes): uint16 length | uint8 type | uint64 seq
    //   'A' (32 bytes):    header + uint64 id | uint8 side | uint32 qty | int64 price
    //   'X' (19 bytes):    header + uint64 id
    //   'E' (23 bytes):    header + uint64 id | uint32 exec_qty
    size_t parse(const uint8_t* buf, size_t len, Handler& handler) {
        (void)buf;
        (void)len;
        (void)handler;
        throw std::logic_error("TODO: FeedParser::parse");
    }

    uint64_t next_expected_seq() const noexcept { return next_seq_; }

private:
    uint64_t next_seq_ = 0;
};
