#pragma once

#include <obz/market_lab/market_data/messages.hpp>

#include <cstdint>
#include <optional>

namespace obz::market_lab::market_data {

struct top_of_book_level {
    std::uint64_t price{};
    std::uint64_t aggregate_size{};

    bool operator==(const top_of_book_level&) const = default;
};

struct top_of_book_snapshot {
    symbol_id instrument;
    std::uint64_t reference_sequence{};
    std::optional<top_of_book_level> best_bid;
    std::optional<top_of_book_level> best_ask;
};

} // namespace obz::market_lab::market_data
