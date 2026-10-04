#pragma once

#include <obz/market_lab/matching/events.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace obz::market_lab::market_data {

struct encoded_datagram {
    std::uint64_t sequence{};
    std::vector<std::byte> bytes;
};

class feed_encoder {
public:
    explicit feed_encoder(std::uint64_t first_sequence = 1);

    std::optional<encoded_datagram> encode(const matching::event& emitted);

private:
    std::uint64_t next_sequence_;
    bool sequence_exhausted_{};
};

} // namespace obz::market_lab::market_data
