#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace obz::market_lab::market_data {

enum class side {
    buy,
    sell
};

struct book_update {
    std::string symbol;
    side direction{};
    std::optional<std::uint64_t> best_price;
    std::uint64_t aggregate_size{};
};

using message = std::variant<book_update>;

} // namespace obz::market_lab::market_data
