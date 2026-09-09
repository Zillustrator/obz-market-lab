#include <obz_market/engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace obz_market;

template <typename T>
const T& require_event(const std::vector<event>& events, std::size_t index) {
    CAPTURE(index);
    CAPTURE(events.size());
    REQUIRE(index < events.size());

    const auto* concrete = std::get_if<T>(&events[index]);
    INFO("event has expected concrete type");
    REQUIRE(concrete != nullptr);

    return *concrete;
}

void require_event_count(const std::vector<event>& events, std::size_t expected) {
    CAPTURE(expected);
    CAPTURE(events.size());
    REQUIRE(events.size() == expected);
}

submit_order make_limit_order(
    std::uint64_t user,
    std::uint64_t client_order,
    std::string instrument,
    side direction,
    std::uint64_t limit_price,
    std::uint64_t size
) {
    return submit_order{
        order_key{user_id{user}, client_order_id{client_order}},
        symbol{std::move(instrument)},
        direction,
        order_pricing{std::in_place_type<limit_order>, price{limit_price}},
        quantity{size}
    };
}

submit_order make_market_order(
    std::uint64_t user,
    std::uint64_t client_order,
    std::string instrument,
    side direction,
    std::uint64_t size
) {
    return submit_order{
        order_key{user_id{user}, client_order_id{client_order}},
        symbol{std::move(instrument)},
        direction,
        order_pricing{std::in_place_type<market_order>},
        quantity{size}
    };
}

update_order make_limit_update(
    std::uint64_t user,
    std::uint64_t client_order,
    std::uint64_t limit_price,
    std::uint64_t size
) {
    return update_order{
        order_key{user_id{user}, client_order_id{client_order}},
        order_pricing{std::in_place_type<limit_order>, price{limit_price}},
        quantity{size}
    };
}

update_order make_market_update(
    std::uint64_t user,
    std::uint64_t client_order,
    std::uint64_t size
) {
    return update_order{
        order_key{user_id{user}, client_order_id{client_order}},
        order_pricing{std::in_place_type<market_order>},
        quantity{size}
    };
}

} // namespace

TEST_CASE("resting limit order updates top of book", "[engine]") {
    matching_engine engine;

    const auto events = engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 100, 7));

    require_event_count(events, 2);
    require_event<submit_accepted>(events, 0);

    const auto& update = require_event<book_updated>(events, 1);
    REQUIRE(update.instrument == symbol{"ETH-USD"});
    REQUIRE(update.direction == side::buy);
    REQUIRE(update.best_price == price{100});
    REQUIRE(update.total_size == 7);
}

TEST_CASE("limit order matches resting opposite side at resting price", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 10));
    const auto events = engine.submit(make_limit_order(2, 20, "ETH-USD", side::buy, 105, 4));

    require_event_count(events, 3);
    require_event<submit_accepted>(events, 0);

    const auto& trade = require_event<trade_executed>(events, 1);
    REQUIRE(trade.aggressor == order_key{user_id{2}, client_order_id{20}});
    REQUIRE(trade.resting == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(trade.execution_price == price{100});
    REQUIRE(trade.executed_size == quantity{4});

    const auto& update = require_event<book_updated>(events, 2);
    REQUIRE(update.direction == side::sell);
    REQUIRE(update.best_price == price{100});
    REQUIRE(update.total_size == 6);
}

TEST_CASE("matching respects price-time priority", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 10));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::sell, 100, 10));
    const auto events = engine.submit(make_limit_order(3, 30, "ETH-USD", side::buy, 100, 15));

    require_event_count(events, 4);

    const auto& first_trade = require_event<trade_executed>(events, 1);
    REQUIRE(first_trade.resting == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(first_trade.executed_size == quantity{10});

    const auto& second_trade = require_event<trade_executed>(events, 2);
    REQUIRE(second_trade.resting == order_key{user_id{2}, client_order_id{20}});
    REQUIRE(second_trade.executed_size == quantity{5});

    const auto& update = require_event<book_updated>(events, 3);
    REQUIRE(update.best_price == price{100});
    REQUIRE(update.total_size == 5);
}

TEST_CASE("limit order crosses multiple price levels", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 3));
    engine.submit(make_limit_order(1, 11, "ETH-USD", side::sell, 101, 4));
    engine.submit(make_limit_order(1, 12, "ETH-USD", side::sell, 102, 5));

    const auto events = engine.submit(make_limit_order(2, 20, "ETH-USD", side::buy, 101, 10));

    require_event_count(events, 5);

    const auto& first_trade = require_event<trade_executed>(events, 1);
    REQUIRE(first_trade.resting == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(first_trade.execution_price == price{100});
    REQUIRE(first_trade.executed_size == quantity{3});

    const auto& second_trade = require_event<trade_executed>(events, 2);
    REQUIRE(second_trade.resting == order_key{user_id{1}, client_order_id{11}});
    REQUIRE(second_trade.execution_price == price{101});
    REQUIRE(second_trade.executed_size == quantity{4});

    const auto& bid_update = require_event<book_updated>(events, 3);
    REQUIRE(bid_update.direction == side::buy);
    REQUIRE(bid_update.best_price == price{101});
    REQUIRE(bid_update.total_size == 3);

    const auto& ask_update = require_event<book_updated>(events, 4);
    REQUIRE(ask_update.direction == side::sell);
    REQUIRE(ask_update.best_price == price{102});
    REQUIRE(ask_update.total_size == 5);

    const auto snapshot = engine.snapshot(symbol{"ETH-USD"}, 2);
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->bids.size() == 1);
    REQUIRE(snapshot->bids[0].level_price == price{101});
    REQUIRE(snapshot->bids[0].total_size == 3);
    REQUIRE(snapshot->asks.size() == 1);
    REQUIRE(snapshot->asks[0].level_price == price{102});
}

TEST_CASE("market order does not rest", "[engine]") {
    matching_engine engine;

    const auto first_events = engine.submit(make_market_order(1, 10, "ETH-USD", side::buy, 5));
    require_event_count(first_events, 1);
    require_event<submit_accepted>(first_events, 0);

    const auto second_events = engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5));
    require_event<submit_accepted>(second_events, 0);
}

TEST_CASE("partially filled market order expires", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 4));

    const auto events = engine.submit(make_market_order(2, 20, "ETH-USD", side::buy, 10));

    require_event_count(events, 3);

    const auto& trade = require_event<trade_executed>(events, 1);
    REQUIRE(trade.resting == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(trade.executed_size == quantity{4});

    const auto& update = require_event<book_updated>(events, 2);
    REQUIRE(update.direction == side::sell);
    REQUIRE(!update.best_price.has_value());
    REQUIRE(update.total_size == 0);

    const auto snapshot = engine.snapshot(symbol{"ETH-USD"}, 1);
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->bids.empty());
    REQUIRE(snapshot->asks.empty());
}

TEST_CASE("cancellation removes resting order", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5));
    const auto events = engine.cancel(cancel_order{order_key{user_id{1}, client_order_id{10}}});

    require_event_count(events, 2);
    require_event<order_cancelled>(events, 0);

    const auto& update = require_event<book_updated>(events, 1);
    REQUIRE(update.direction == side::buy);
    REQUIRE(!update.best_price.has_value());
    REQUIRE(update.total_size == 0);
}

TEST_CASE("partially filled resting order can be cancelled", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 10));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::buy, 100, 4));

    const auto events = engine.cancel(cancel_order{order_key{user_id{1}, client_order_id{10}}});

    require_event_count(events, 2);
    require_event<order_cancelled>(events, 0);

    const auto& update = require_event<book_updated>(events, 1);
    REQUIRE(update.direction == side::sell);
    REQUIRE(!update.best_price.has_value());
    REQUIRE(update.total_size == 0);
}

TEST_CASE("cancel after full fill is rejected", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 5));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::buy, 100, 5));

    const auto events = engine.cancel(cancel_order{order_key{user_id{1}, client_order_id{10}}});

    require_event_count(events, 1);
    const auto& rejection = require_event<cancel_rejected>(events, 0);
    REQUIRE(rejection.reason == "order is not active");
}

TEST_CASE("books are separated by symbol", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 5));
    engine.submit(make_limit_order(2, 20, "BTC-USD", side::sell, 100, 5));
    const auto events = engine.submit(make_limit_order(3, 30, "ETH-USD", side::buy, 100, 5));

    require_event_count(events, 3);
    const auto& trade = require_event<trade_executed>(events, 1);
    REQUIRE(trade.instrument == symbol{"ETH-USD"});
    REQUIRE(trade.resting == order_key{user_id{1}, client_order_id{10}});
}

TEST_CASE("invalid and duplicate orders are rejected", "[engine]") {
    matching_engine engine;

    const submit_order invalid_order{
        order_key{user_id{1}, client_order_id{10}},
        symbol{},
        side::buy,
        order_pricing{std::in_place_type<limit_order>, price{100}},
        quantity{5}
    };

    const auto invalid_events = engine.submit(invalid_order);
    require_event_count(invalid_events, 1);
    const auto& invalid_rejection = require_event<submit_rejected>(invalid_events, 0);
    REQUIRE(invalid_rejection.reason == "symbol must not be empty");

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5));
    const auto duplicate_events = engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 99, 5));
    const auto& rejection = require_event<submit_rejected>(duplicate_events, 0);
    REQUIRE(rejection.reason == "order key is already active");
}

TEST_CASE("inactive cancel is rejected", "[engine]") {
    matching_engine engine;

    const auto events = engine.cancel(cancel_order{order_key{user_id{1}, client_order_id{10}}});

    require_event_count(events, 1);
    const auto& rejection = require_event<cancel_rejected>(events, 0);
    REQUIRE(rejection.reason == "order is not active");
}

TEST_CASE("reducing resting order size preserves time priority", "[engine][update]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 5));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::sell, 100, 5));

    const auto update_events = engine.update(make_limit_update(1, 10, 100, 3));

    require_event_count(update_events, 2);
    require_event<update_accepted>(update_events, 0);

    const auto& update = require_event<book_updated>(update_events, 1);
    REQUIRE(update.direction == side::sell);
    REQUIRE(update.best_price == price{100});
    REQUIRE(update.total_size == 8);

    const auto match_events = engine.submit(make_limit_order(3, 30, "ETH-USD", side::buy, 100, 4));

    require_event_count(match_events, 4);

    const auto& first_trade = require_event<trade_executed>(match_events, 1);
    REQUIRE(first_trade.resting == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(first_trade.executed_size == quantity{3});

    const auto& second_trade = require_event<trade_executed>(match_events, 2);
    REQUIRE(second_trade.resting == order_key{user_id{2}, client_order_id{20}});
    REQUIRE(second_trade.executed_size == quantity{1});
}

TEST_CASE("increasing resting order size loses time priority", "[engine][update]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 5));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::sell, 100, 5));

    const auto update_events = engine.update(make_limit_update(1, 10, 100, 6));

    require_event_count(update_events, 2);
    require_event<update_accepted>(update_events, 0);

    const auto& update = require_event<book_updated>(update_events, 1);
    REQUIRE(update.direction == side::sell);
    REQUIRE(update.best_price == price{100});
    REQUIRE(update.total_size == 11);

    const auto match_events = engine.submit(make_limit_order(3, 30, "ETH-USD", side::buy, 100, 5));

    require_event_count(match_events, 3);

    const auto& trade = require_event<trade_executed>(match_events, 1);
    REQUIRE(trade.resting == order_key{user_id{2}, client_order_id{20}});
    REQUIRE(trade.executed_size == quantity{5});
}

TEST_CASE("repricing resting order can cross the book", "[engine][update]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::sell, 105, 5));

    const auto update_events = engine.update(make_limit_update(2, 20, 100, 5));

    require_event_count(update_events, 4);
    require_event<update_accepted>(update_events, 0);

    const auto& trade = require_event<trade_executed>(update_events, 1);
    REQUIRE(trade.aggressor == order_key{user_id{2}, client_order_id{20}});
    REQUIRE(trade.resting == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(trade.execution_price == price{100});
    REQUIRE(trade.executed_size == quantity{5});

    const auto& ask_update = require_event<book_updated>(update_events, 2);
    REQUIRE(ask_update.direction == side::sell);
    REQUIRE_FALSE(ask_update.best_price.has_value());

    const auto& bid_update = require_event<book_updated>(update_events, 3);
    REQUIRE(bid_update.direction == side::buy);
    REQUIRE_FALSE(bid_update.best_price.has_value());

    const auto cancel_updated = engine.cancel(cancel_order{order_key{user_id{2}, client_order_id{20}}});
    const auto& rejection = require_event<cancel_rejected>(cancel_updated, 0);
    REQUIRE(rejection.reason == "order is not active");
}

TEST_CASE("inactive update is rejected", "[engine][update]") {
    matching_engine engine;

    const auto events = engine.update(make_limit_update(1, 10, 100, 5));

    require_event_count(events, 1);
    const auto& rejection = require_event<update_rejected>(events, 0);
    REQUIRE(rejection.reason == "order is not active");
}

TEST_CASE("market-priced update is rejected", "[engine][update]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5));

    const auto events = engine.update(make_market_update(1, 10, 5));

    require_event_count(events, 1);
    const auto& rejection = require_event<update_rejected>(events, 0);
    REQUIRE(rejection.reason == "update pricing must be limit");
}

TEST_CASE("snapshots return deterministic depth levels", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 99, 3));
    engine.submit(make_limit_order(1, 11, "ETH-USD", side::buy, 101, 5));
    engine.submit(make_limit_order(1, 12, "ETH-USD", side::buy, 100, 7));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::sell, 105, 11));
    engine.submit(make_limit_order(2, 21, "ETH-USD", side::sell, 103, 13));
    engine.submit(make_limit_order(2, 22, "ETH-USD", side::sell, 104, 17));

    const auto snapshot = engine.snapshot(symbol{"ETH-USD"}, 2);

    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->instrument == symbol{"ETH-USD"});
    REQUIRE(snapshot->bids.size() == 2);
    REQUIRE(snapshot->asks.size() == 2);

    REQUIRE(snapshot->bids[0].level_price == price{101});
    REQUIRE(snapshot->bids[0].total_size == 5);
    REQUIRE(snapshot->bids[1].level_price == price{100});
    REQUIRE(snapshot->bids[1].total_size == 7);

    REQUIRE(snapshot->asks[0].level_price == price{103});
    REQUIRE(snapshot->asks[0].total_size == 13);
    REQUIRE(snapshot->asks[1].level_price == price{104});
    REQUIRE(snapshot->asks[1].total_size == 17);
}

TEST_CASE("zero depth snapshot returns empty sides", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::sell, 105, 7));

    const auto snapshot = engine.snapshot(symbol{"ETH-USD"}, 0);

    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->bids.empty());
    REQUIRE(snapshot->asks.empty());
}

TEST_CASE("snapshot reflects partial fill and cancel", "[engine]") {
    matching_engine engine;

    engine.submit(make_limit_order(1, 10, "ETH-USD", side::sell, 100, 10));
    engine.submit(make_limit_order(2, 20, "ETH-USD", side::buy, 100, 4));

    const auto partial_snapshot = engine.snapshot(symbol{"ETH-USD"}, 1);
    REQUIRE(partial_snapshot.has_value());
    REQUIRE(partial_snapshot->asks.size() == 1);
    REQUIRE(partial_snapshot->asks[0].level_price == price{100});
    REQUIRE(partial_snapshot->asks[0].total_size == 6);

    engine.cancel(cancel_order{order_key{user_id{1}, client_order_id{10}}});

    const auto cancelled_snapshot = engine.snapshot(symbol{"ETH-USD"}, 1);
    REQUIRE(cancelled_snapshot.has_value());
    REQUIRE(cancelled_snapshot->bids.empty());
    REQUIRE(cancelled_snapshot->asks.empty());
}

TEST_CASE("missing symbol snapshot is empty", "[engine]") {
    matching_engine engine;

    const auto snapshot = engine.snapshot(symbol{"ETH-USD"}, 1);

    REQUIRE(!snapshot.has_value());
}
