#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <variant>

namespace obz::market_lab::market_data {

enum class side {
    buy,
    sell
};

struct symbol_id {
    std::uint32_t value{};

    constexpr symbol_id() = default;
    constexpr explicit symbol_id(std::uint32_t id) noexcept
        : value(id) {}

    bool operator==(const symbol_id&) const = default;
    auto operator<=>(const symbol_id&) const = default;
};

struct book_update {
    symbol_id instrument;
    side direction{};
    std::optional<std::uint64_t> best_price;
    std::uint64_t aggregate_size{};
};

using message = std::variant<book_update>;

} // namespace obz::market_lab::market_data
