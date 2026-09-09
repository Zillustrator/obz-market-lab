#include <obz_market/matching_engine.hpp>

#include <obz_market/detail/tracing.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace obz_market {

std::vector<event> matching_engine::submit(const submit_order& order) {
    OBZ_MARKET_TRACE_SCOPE();

    if (auto reason = validate(order); reason.has_value()) {
        return reject_submit(order.key, *reason);
    }

    if (active_orders_.contains(order.key)) {
        return reject_submit(order.key, "order key is already active");
    }

    auto& book = get_or_create_book(order.instrument);
    auto result = book.submit(order, next_sequence_++);

    record_submit_result(order, result);

    return std::move(result.events);
}

std::vector<event> matching_engine::update(const update_order& order) {
    OBZ_MARKET_TRACE_SCOPE();

    if (auto reason = validate(order); reason.has_value()) {
        return reject_update(order.key, *reason);
    }

    auto active_it = active_orders_.find(order.key);
    if (active_it == active_orders_.end()) {
        return reject_update(order.key, "order is not active");
    }

    const auto& active_order = active_it->second;
    auto* book = find_book(active_order.instrument);
    if (book == nullptr) {
        active_orders_.erase(active_it);
        return reject_update(order.key, "order book is missing");
    }

    auto result = book->update(order, active_order.handle, next_sequence_++);
    if (!result.updated) {
        active_orders_.erase(active_it);
        return reject_update(order.key, "order book is inconsistent");
    }

    record_update_result(active_it, result);

    return std::move(result.events);
}

std::vector<event> matching_engine::cancel(const cancel_order& order) {
    OBZ_MARKET_TRACE_SCOPE();

    const auto active_it = active_orders_.find(order.key);
    if (active_it == active_orders_.end()) {
        return reject_cancel(order.key, "order is not active");
    }

    const auto& active_order = active_it->second;
    auto* book = find_book(active_order.instrument);
    if (book == nullptr) {
        active_orders_.erase(active_it);
        return reject_cancel(order.key, "order book is missing");
    }

    auto result = book->cancel(order.key, active_order.handle);

    record_cancel_result(active_it, result);

    return std::move(result.events);
}

std::optional<book_snapshot> matching_engine::snapshot(
    const symbol& instrument,
    std::size_t depth
) const {
    OBZ_MARKET_TRACE_SCOPE();

    const auto* book = find_book(instrument);
    if (book == nullptr) {
        return std::nullopt;
    }

    return book->snapshot(depth);
}

std::size_t matching_engine::order_key_hash::operator()(const order_key& key) const noexcept {
    const auto user_hash = std::hash<std::uint64_t>{}(key.user.value);
    const auto order_hash = std::hash<std::uint64_t>{}(key.client_order.value);
    return user_hash ^ (order_hash + 0x9e3779b97f4a7c15ULL + (user_hash << 6U) + (user_hash >> 2U));
}

std::size_t matching_engine::symbol_hash::operator()(const symbol& instrument) const noexcept {
    return std::hash<std::string>{}(instrument.value);
}

std::optional<std::string> matching_engine::validate(const submit_order& order) {
    if (order.key.user.value == 0) {
        return "user id must be non-zero";
    }

    if (order.key.client_order.value == 0) {
        return "client order id must be non-zero";
    }

    if (order.instrument.empty()) {
        return "symbol must not be empty";
    }

    if (order.size.value == 0) {
        return "order size must be positive";
    }

    return std::nullopt;
}

std::optional<std::string> matching_engine::validate(const update_order& order) {
    if (order.key.user.value == 0) {
        return "user id must be non-zero";
    }

    if (order.key.client_order.value == 0) {
        return "client order id must be non-zero";
    }

    if (order.size.value == 0) {
        return "order size must be positive";
    }

    if (!std::holds_alternative<limit_order>(order.pricing)) {
        return "update pricing must be limit";
    }

    return std::nullopt;
}

std::vector<event> matching_engine::reject_submit(const order_key& key, std::string reason) {
    return std::vector<event>{
        event{std::in_place_type<submit_rejected>, key, std::move(reason)}
    };
}

std::vector<event> matching_engine::reject_update(const order_key& key, std::string reason) {
    return std::vector<event>{
        event{std::in_place_type<update_rejected>, key, std::move(reason)}
    };
}

std::vector<event> matching_engine::reject_cancel(const order_key& key, std::string reason) {
    return std::vector<event>{
        event{std::in_place_type<cancel_rejected>, key, std::move(reason)}
    };
}

order_book& matching_engine::get_or_create_book(const symbol& instrument) {
    auto [it, inserted] = books_.try_emplace(instrument, instrument);
    static_cast<void>(inserted);
    return it->second;
}

order_book* matching_engine::find_book(const symbol& instrument) {
    auto it = books_.find(instrument);
    if (it == books_.end()) {
        return nullptr;
    }

    return &it->second;
}

const order_book* matching_engine::find_book(const symbol& instrument) const {
    auto it = books_.find(instrument);
    if (it == books_.end()) {
        return nullptr;
    }

    return &it->second;
}

void matching_engine::record_submit_result(
    const submit_order& order,
    const order_book::submit_result& result
) {
    for (const auto& filled_key : result.filled_resting_orders) {
        active_orders_.erase(filled_key);
    }

    if (result.resting_order) {
        active_orders_.emplace(
            order.key,
            active_order{
                order.instrument,
                result.resting_order
            }
        );
    }
}

void matching_engine::record_update_result(
    active_order_map::iterator active,
    const order_book::update_result& result
) {
    for (const auto& filled_key : result.filled_resting_orders) {
        active_orders_.erase(filled_key);
    }

    if (result.resting_order) {
        active->second.handle = result.resting_order;
    } else {
        active_orders_.erase(active);
    }
}

void matching_engine::record_cancel_result(
    active_order_map::iterator active,
    const order_book::cancel_result& result
) {
    if (result.cancelled) {
        active_orders_.erase(active);
    }
}

} // namespace obz_market
