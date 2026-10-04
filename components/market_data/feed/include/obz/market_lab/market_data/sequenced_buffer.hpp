#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <utility>

namespace obz::market_lab::market_data {

struct sequenced_buffer_limits {
    std::size_t maximum_pending_values{};
    std::uint64_t maximum_sequence_gap{};
};

enum class sequence_result {
    ready,
    buffered,
    old,
    duplicate,
    gap_limit_exceeded,
    capacity_exhausted
};

template <typename T>
class sequenced_buffer {
public:
    explicit sequenced_buffer(
        sequenced_buffer_limits limits,
        std::optional<std::uint64_t> first_sequence = std::nullopt)
        : limits_{limits},
          next_sequence_{first_sequence} {}

    template <typename Factory>
    sequence_result push(std::uint64_t sequence, Factory&& make_value) {
        if (sequence_exhausted_) {
            return sequence_result::old;
        }

        if (!next_sequence_.has_value()) {
            advance_after(sequence);
            return sequence_result::ready;
        }

        if (sequence < *next_sequence_) {
            return sequence_result::old;
        }

        if (sequence == *next_sequence_) {
            advance_after(sequence);
            return sequence_result::ready;
        }

        if (pending_.contains(sequence)) {
            return sequence_result::duplicate;
        }

        if (sequence - *next_sequence_ > limits_.maximum_sequence_gap) {
            return sequence_result::gap_limit_exceeded;
        }

        if (pending_.size() >= limits_.maximum_pending_values) {
            return sequence_result::capacity_exhausted;
        }

        pending_.emplace(sequence, std::invoke(std::forward<Factory>(make_value)));
        return sequence_result::buffered;
    }

    std::optional<T> pop_ready() {
        if (!next_sequence_.has_value() || sequence_exhausted_ || pending_.empty()) {
            return std::nullopt;
        }

        auto first = pending_.begin();
        if (first->first != *next_sequence_) {
            return std::nullopt;
        }

        const auto sequence = first->first;
        std::optional<T> result{std::in_place, std::move(first->second)};
        pending_.erase(first);
        advance_after(sequence);
        return result;
    }

    std::optional<std::uint64_t> next_sequence() const noexcept {
        return next_sequence_;
    }

    std::size_t pending_size() const noexcept {
        return pending_.size();
    }

private:
    void advance_after(std::uint64_t sequence) noexcept {
        if (sequence == std::numeric_limits<std::uint64_t>::max()) {
            sequence_exhausted_ = true;
            next_sequence_.reset();
            return;
        }

        next_sequence_ = sequence + 1;
    }

    sequenced_buffer_limits limits_;
    std::optional<std::uint64_t> next_sequence_;
    std::map<std::uint64_t, T> pending_;
    bool sequence_exhausted_{};
};

} // namespace obz::market_lab::market_data
