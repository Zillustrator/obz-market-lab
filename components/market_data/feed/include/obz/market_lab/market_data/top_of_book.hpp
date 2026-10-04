#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace obz::market_lab::market_data {

struct top_of_book_level {
    std::uint64_t price{};
    std::uint64_t aggregate_size{};

    bool operator==(const top_of_book_level&) const = default;
};

struct top_of_book_snapshot {
    std::string symbol;
    std::uint64_t reference_sequence{};
    std::optional<top_of_book_level> best_bid;
    std::optional<top_of_book_level> best_ask;
};

} // namespace obz::market_lab::market_data
