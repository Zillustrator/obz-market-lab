#include <obz_market/order_book.hpp>

#include <obz_market/detail/tracing.hpp>

#include <algorithm>
#include <cstdint>
#include <variant>
#include <utility>

namespace obz_market {

order_book::order_book(symbol instrument)
    : instrument_(std::move(instrument)) {}

order_book::submit_result order_book::submit(
    const submit_order& order,
    sequence_number sequence
) {
    OBZ_MARKET_TRACE_SCOPE();

    if (order.direction == side::buy) {
        return submit_buy(order, sequence);
    }

    if (order.direction == side::sell) {
        return submit_sell(order, sequence);
    }

    return {};
}

order_book::update_result order_book::update(
    const update_order& order,
    const resting_order_handle& handle,
    sequence_number sequence
) {
    OBZ_MARKET_TRACE_SCOPE();

    if (handle.direction_ == side::buy) {
        return update_buy(order, handle, sequence);
    }

    if (handle.direction_ == side::sell) {
        return update_sell(order, handle, sequence);
    }

    return {};
}

order_book::cancel_result order_book::cancel(
    const order_key& key,
    const resting_order_handle& handle
) {
    OBZ_MARKET_TRACE_SCOPE();

    if (handle.direction_ == side::buy) {
        return cancel_bid(key, handle);
    }

    if (handle.direction_ == side::sell) {
        return cancel_ask(key, handle);
    }

    return {};
}

order_book::order_book_snapshot order_book::snapshot(std::size_t depth) const {
    OBZ_MARKET_TRACE_SCOPE();

    return order_book_snapshot{
        instrument_,
        snapshot_levels(bids_, depth),
        snapshot_levels(asks_, depth)
    };
}

order_book::submit_result order_book::submit_buy(
    const submit_order& order,
    sequence_number sequence
) {
    OBZ_MARKET_TRACE_SCOPE();

    submit_result result;
    auto& events = result.events;

    const auto best_ask_before = top_of_book(asks_);

    events.emplace_back(submit_accepted{order.key});

    incoming_order incoming{
        order.key,
        order.direction,
        order.size,
        sequence
    };

    match_order(
        order.pricing,
        can_buy_cross,
        incoming,
        asks_,
        events,
        result.filled_resting_orders
    );

    if (incoming.open_size.value > 0) {
        if (const auto resting_price = limit_price(order.pricing); resting_price.has_value()) {
            const auto best_bid_before = top_of_book(bids_);
            result.resting_order = rest_order(incoming, *resting_price);
            append_book_update_if_changed(side::buy, best_bid_before, top_of_book(bids_), events);
        }
    }

    append_book_update_if_changed(side::sell, best_ask_before, top_of_book(asks_), events);

    return result;
}

order_book::submit_result order_book::submit_sell(
    const submit_order& order,
    sequence_number sequence
) {
    OBZ_MARKET_TRACE_SCOPE();

    submit_result result;
    auto& events = result.events;

    const auto best_bid_before = top_of_book(bids_);

    events.emplace_back(submit_accepted{order.key});

    incoming_order incoming{
        order.key,
        order.direction,
        order.size,
        sequence
    };

    match_order(
        order.pricing,
        can_sell_cross,
        incoming,
        bids_,
        events,
        result.filled_resting_orders
    );

    append_book_update_if_changed(side::buy, best_bid_before, top_of_book(bids_), events);

    if (incoming.open_size.value > 0) {
        if (const auto resting_price = limit_price(order.pricing); resting_price.has_value()) {
            const auto best_ask_before = top_of_book(asks_);
            result.resting_order = rest_order(incoming, *resting_price);
            append_book_update_if_changed(side::sell, best_ask_before, top_of_book(asks_), events);
        }
    }

    return result;
}

order_book::update_result order_book::update_buy(
    const update_order& order,
    const resting_order_handle& handle,
    sequence_number sequence
) {
    OBZ_MARKET_TRACE_SCOPE();

    const auto best_before = top_of_book(bids_);

    if (reduce_order_size(order, handle, bids_)) {
        update_result result;
        result.updated = true;
        result.resting_order = handle;
        result.events.emplace_back(update_accepted{order.key});
        append_book_update_if_changed(side::buy, best_before, top_of_book(bids_), result.events);
        return result;
    }

    return replace_order(
        order,
        handle,
        side::buy,
        can_buy_cross,
        bids_,
        asks_,
        sequence
    );
}

order_book::update_result order_book::update_sell(
    const update_order& order,
    const resting_order_handle& handle,
    sequence_number sequence
) {
    OBZ_MARKET_TRACE_SCOPE();

    const auto best_before = top_of_book(asks_);

    if (reduce_order_size(order, handle, asks_)) {
        update_result result;
        result.updated = true;
        result.resting_order = handle;
        result.events.emplace_back(update_accepted{order.key});
        append_book_update_if_changed(side::sell, best_before, top_of_book(asks_), result.events);
        return result;
    }

    return replace_order(
        order,
        handle,
        side::sell,
        can_sell_cross,
        asks_,
        bids_,
        sequence
    );
}

order_book::cancel_result order_book::cancel_bid(
    const order_key& key,
    const resting_order_handle& handle
) {
    OBZ_MARKET_TRACE_SCOPE();

    cancel_result result;
    const auto best_before = top_of_book(bids_);

    if (cancel_from_levels(key, handle, bids_, result.events)) {
        result.cancelled = true;
        append_book_update_if_changed(side::buy, best_before, top_of_book(bids_), result.events);
    }

    return result;
}

order_book::cancel_result order_book::cancel_ask(
    const order_key& key,
    const resting_order_handle& handle
) {
    OBZ_MARKET_TRACE_SCOPE();

    cancel_result result;
    const auto best_before = top_of_book(asks_);

    if (cancel_from_levels(key, handle, asks_, result.events)) {
        result.cancelled = true;
        append_book_update_if_changed(side::sell, best_before, top_of_book(asks_), result.events);
    }

    return result;
}

std::optional<price> order_book::limit_price(const order_pricing& pricing) {
    if (const auto* limit = std::get_if<limit_order>(&pricing)) {
        return limit->limit_price;
    }

    return std::nullopt;
}

bool order_book::can_buy_cross(const order_pricing& pricing, price best_ask) {
    if (const auto limit = limit_price(pricing); limit.has_value()) {
        return *limit >= best_ask;
    }

    return true;
}

bool order_book::can_sell_cross(const order_pricing& pricing, price best_bid) {
    if (const auto limit = limit_price(pricing); limit.has_value()) {
        return *limit <= best_bid;
    }

    return true;
}

template <typename Levels>
bool order_book::cancel_from_levels(
    const order_key& key,
    const resting_order_handle& handle,
    Levels& levels,
    std::vector<event>& events
) {
    OBZ_MARKET_TRACE_SCOPE();

    if (!handle.limit_price_.has_value()) {
        return false;
    }

    auto level_it = levels.find(*handle.limit_price_);
    if (level_it == levels.end()) {
        return false;
    }

    auto& level = level_it->second;
    if (handle.order_->key != key) {
        return false;
    }

    level.total_size -= handle.order_->open_size.value;
    level.orders.erase(handle.order_);

    if (level.orders.empty()) {
        levels.erase(level_it);
    }

    events.emplace_back(order_cancelled{key});
    return true;
}

template <typename Levels>
bool order_book::reduce_order_size(
    const update_order& order,
    const resting_order_handle& handle,
    Levels& levels
) {
    OBZ_MARKET_TRACE_SCOPE();

    const auto new_price = limit_price(order.pricing);
    if (!new_price.has_value() || !handle.limit_price_.has_value()) {
        return false;
    }

    if (*new_price != *handle.limit_price_) {
        return false;
    }

    auto level_it = levels.find(*handle.limit_price_);
    if (level_it == levels.end()) {
        return false;
    }

    auto& resting = *handle.order_;
    if (resting.key != order.key || order.size > resting.open_size) {
        return false;
    }

    auto& level = level_it->second;
    level.total_size -= resting.open_size.value - order.size.value;
    resting.open_size = order.size;

    return true;
}

template <typename Levels>
bool order_book::remove_order(
    const order_key& key,
    const resting_order_handle& handle,
    Levels& levels
) {
    OBZ_MARKET_TRACE_SCOPE();

    if (!handle.limit_price_.has_value()) {
        return false;
    }

    auto level_it = levels.find(*handle.limit_price_);
    if (level_it == levels.end()) {
        return false;
    }

    auto& level = level_it->second;
    if (handle.order_->key != key) {
        return false;
    }

    level.total_size -= handle.order_->open_size.value;
    level.orders.erase(handle.order_);

    if (level.orders.empty()) {
        levels.erase(level_it);
    }

    return true;
}

template <typename CanCross, typename OwnLevels, typename OppositeLevels>
order_book::update_result order_book::replace_order(
    const update_order& order,
    const resting_order_handle& handle,
    side direction,
    CanCross can_cross,
    OwnLevels& own_levels,
    OppositeLevels& opposite_levels,
    sequence_number sequence
) {
    OBZ_MARKET_TRACE_SCOPE();

    update_result result;
    auto& events = result.events;

    const auto own_best_before = top_of_book(own_levels);
    const auto opposite_best_before = top_of_book(opposite_levels);

    if (!remove_order(order.key, handle, own_levels)) {
        return result;
    }

    result.updated = true;
    events.emplace_back(update_accepted{order.key});

    incoming_order incoming{
        order.key,
        direction,
        order.size,
        sequence
    };

    match_order(
        order.pricing,
        can_cross,
        incoming,
        opposite_levels,
        events,
        result.filled_resting_orders
    );

    if (incoming.open_size.value > 0) {
        if (const auto resting_price = limit_price(order.pricing); resting_price.has_value()) {
            result.resting_order = rest_order(incoming, *resting_price);
        }
    }

    append_book_update_if_changed(direction, own_best_before, top_of_book(own_levels), events);
    append_book_update_if_changed(
        direction == side::buy ? side::sell : side::buy,
        opposite_best_before,
        top_of_book(opposite_levels),
        events
    );

    return result;
}

template <typename CanCross, typename Levels>
void order_book::match_order(
    const order_pricing& pricing,
    CanCross can_cross,
    incoming_order& incoming,
    Levels& levels,
    std::vector<event>& events,
    std::vector<order_key>& filled_resting_orders
) {
    OBZ_MARKET_TRACE_SCOPE();

    while (incoming.open_size.value > 0 && !levels.empty()) {
        auto best_level_it = levels.begin();

        if (!can_cross(pricing, best_level_it->first)) {
            break;
        }

        auto& level = best_level_it->second;
        match_against_level(incoming, level, events, filled_resting_orders);

        if (level.orders.empty()) {
            levels.erase(best_level_it);
        }
    }
}

void order_book::match_against_level(
    incoming_order& incoming,
    price_level& level,
    std::vector<event>& events,
    std::vector<order_key>& filled_resting_orders
) {
    OBZ_MARKET_TRACE_SCOPE();

    while (incoming.open_size.value > 0 && !level.orders.empty()) {
        auto& resting = level.orders.front();
        const std::uint64_t executed_size = std::min(
            incoming.open_size.value,
            resting.open_size.value
        );

        events.emplace_back(trade_executed{
            incoming.key,
            resting.key,
            instrument_,
            resting.limit_price,
            quantity{executed_size}
        });

        incoming.open_size.value -= executed_size;
        resting.open_size.value -= executed_size;
        level.total_size -= executed_size;

        if (resting.open_size.value == 0) {
            filled_resting_orders.push_back(resting.key);
            level.orders.pop_front();
        }
    }
}

order_book::resting_order_handle order_book::rest_order(
    const incoming_order& incoming,
    price limit_price
) {
    OBZ_MARKET_TRACE_SCOPE();

    const side direction = incoming.direction;

    auto& level = direction == side::buy
        ? bids_[limit_price]
        : asks_[limit_price];

    level.total_size += incoming.open_size.value;
    level.orders.push_back(resting_order{
        incoming.key,
        incoming.direction,
        limit_price,
        incoming.open_size,
        incoming.sequence
    });

    auto order_it = level.orders.end();
    --order_it;

    return resting_order_handle{
        direction,
        limit_price,
        order_it
    };
}

template <typename Levels>
std::optional<order_book::top_of_book_state> order_book::top_of_book(const Levels& levels) const {
    if (levels.empty()) {
        return std::nullopt;
    }

    const auto& [best_price, level] = *levels.begin();
    return top_of_book_state{best_price, level.total_size};
}

template <typename Levels>
std::vector<order_book::price_level_snapshot> order_book::snapshot_levels(
    const Levels& levels,
    std::size_t depth
) {
    OBZ_MARKET_TRACE_SCOPE();

    std::vector<price_level_snapshot> result;
    result.reserve(std::min(depth, levels.size()));

    for (const auto& [level_price, level] : levels) {
        if (result.size() == depth) {
            break;
        }

        result.push_back(price_level_snapshot{
            level_price,
            level.total_size
        });
    }

    return result;
}

void order_book::append_book_update_if_changed(
    side direction,
    const std::optional<top_of_book_state>& before,
    const std::optional<top_of_book_state>& after,
    std::vector<event>& events
) const {
    if (before == after) {
        return;
    }

    if (after.has_value()) {
        events.emplace_back(book_updated{
            instrument_,
            direction,
            after->best_price,
            after->total_size
        });
    } else {
        events.emplace_back(book_updated{
            instrument_,
            direction,
            std::nullopt,
            0
        });
    }
}

} // namespace obz_market
