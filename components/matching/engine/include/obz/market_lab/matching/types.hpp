#pragma once

#include <compare>
#include <cstdint>
#include <stdexcept>

namespace obz::market_lab::matching {

struct symbol_id {
    std::uint32_t value{};

    constexpr symbol_id() = default;
    constexpr explicit symbol_id(std::uint32_t id) noexcept
        : value(id) {}

    bool operator==(const symbol_id&) const = default;
    auto operator<=>(const symbol_id&) const = default;
};

struct user_id {
    std::uint64_t value{};

    constexpr user_id() = default;
    constexpr explicit user_id(std::uint64_t id) noexcept
        : value(id) {}

    bool operator==(const user_id&) const = default;
    auto operator<=>(const user_id&) const = default;
};

struct client_order_id {
    std::uint64_t value{};

    constexpr client_order_id() = default;
    constexpr explicit client_order_id(std::uint64_t id) noexcept
        : value(id) {}

    bool operator==(const client_order_id&) const = default;
    auto operator<=>(const client_order_id&) const = default;
};

struct price {
    std::uint64_t value;

    explicit price(std::uint64_t amount)
        : value(amount) {
        if (value == 0) {
            throw std::invalid_argument("price must be positive");
        }
    }

    bool operator==(const price&) const = default;
    auto operator<=>(const price&) const = default;
};

struct quantity {
    std::uint64_t value;

    explicit quantity(std::uint64_t amount)
        : value(amount) {
        if (value == 0) {
            throw std::invalid_argument("quantity must be positive");
        }
    }

    bool operator==(const quantity&) const = default;
    auto operator<=>(const quantity&) const = default;
};

struct sequence_number {
    std::uint64_t value{};

    constexpr sequence_number() = default;
    constexpr explicit sequence_number(std::uint64_t sequence) noexcept
        : value(sequence) {}

    sequence_number& operator++() noexcept {
        ++value;
        return *this;
    }

    sequence_number operator++(int) noexcept {
        sequence_number previous{*this};
        ++(*this);
        return previous;
    }

    bool operator==(const sequence_number&) const = default;
    auto operator<=>(const sequence_number&) const = default;
};

enum class side {
    buy,
    sell
};

} // namespace obz::market_lab::matching
