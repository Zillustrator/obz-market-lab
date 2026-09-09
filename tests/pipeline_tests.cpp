#include <obz_market/pipeline.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace obz_market;
using namespace std::chrono_literals;

template <typename T>
T get_ready(std::future<T>& future) {
    REQUIRE(future.wait_for(1s) == std::future_status::ready);
    return future.get();
}

template <typename T>
T get_ready(std::future<T>&& future) {
    return get_ready(future);
}

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

} // namespace

TEST_CASE("engine runner requires explicit start", "[pipeline]") {
    engine_runner runner{8};

    REQUIRE_FALSE(runner.running());
    REQUIRE_THROWS_AS(
        runner.submit(submit_command{make_limit_order(1, 10, "ETH-USD", side::buy, 100, 1)}),
        std::runtime_error
    );
}

TEST_CASE("engine runner submits commands through queue", "[pipeline]") {
    engine_runner runner{8};
    runner.start();

    auto future = runner.submit(submit_command{make_limit_order(1, 10, "ETH-USD", side::buy, 100, 7)});
    const auto events = get_ready(future);

    require_event_count(events, 2);
    require_event<submit_accepted>(events, 0);

    const auto& update = require_event<book_updated>(events, 1);
    REQUIRE(update.instrument == symbol{"ETH-USD"});
    REQUIRE(update.direction == side::buy);
    REQUIRE(update.best_price == price{100});
    REQUIRE(update.total_size == 7);

    runner.stop();
    REQUIRE_FALSE(runner.running());
}

TEST_CASE("engine runner preserves command ordering", "[pipeline]") {
    engine_runner runner{8};
    runner.start();

    auto resting = runner.submit(submit_command{make_limit_order(1, 10, "ETH-USD", side::sell, 100, 5)});
    auto crossing = runner.submit(submit_command{make_limit_order(2, 20, "ETH-USD", side::buy, 100, 5)});

    require_event<submit_accepted>(get_ready(resting), 0);

    const auto crossing_events = get_ready(crossing);
    require_event_count(crossing_events, 3);

    const auto& trade = require_event<trade_executed>(crossing_events, 1);
    REQUIRE(trade.aggressor == order_key{user_id{2}, client_order_id{20}});
    REQUIRE(trade.resting == order_key{user_id{1}, client_order_id{10}});

    runner.stop();
}

TEST_CASE("engine runner cancels resting orders through queue", "[pipeline]") {
    engine_runner runner{8};
    runner.start();

    require_event<submit_accepted>(
        get_ready(runner.submit(submit_command{make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5)})),
        0
    );

    auto cancel_future = runner.cancel(cancel_command{cancel_order{
        order_key{user_id{1}, client_order_id{10}}
    }});
    const auto cancel_events = get_ready(cancel_future);

    require_event_count(cancel_events, 2);
    require_event<order_cancelled>(cancel_events, 0);

    const auto& update = require_event<book_updated>(cancel_events, 1);
    REQUIRE(update.direction == side::buy);
    REQUIRE_FALSE(update.best_price.has_value());
    REQUIRE(update.total_size == 0);

    runner.stop();
}

TEST_CASE("engine runner updates resting orders through queue", "[pipeline]") {
    engine_runner runner{8};
    runner.start();

    require_event<submit_accepted>(
        get_ready(runner.submit(submit_command{make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5)})),
        0
    );

    auto update_future = runner.update(update_command{make_limit_update(1, 10, 100, 3)});
    const auto update_events = get_ready(update_future);

    require_event_count(update_events, 2);
    require_event<update_accepted>(update_events, 0);

    const auto& update = require_event<book_updated>(update_events, 1);
    REQUIRE(update.direction == side::buy);
    REQUIRE(update.best_price == price{100});
    REQUIRE(update.total_size == 3);

    runner.stop();
}

TEST_CASE("engine runner snapshots through queue", "[pipeline]") {
    engine_runner runner{8};
    runner.start();

    require_event<submit_accepted>(
        get_ready(runner.submit(submit_command{make_limit_order(1, 10, "ETH-USD", side::buy, 100, 5)})),
        0
    );

    auto snapshot_future = runner.snapshot(snapshot_command{symbol{"ETH-USD"}, 1});
    const auto snapshot = get_ready(snapshot_future);

    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->instrument == symbol{"ETH-USD"});
    REQUIRE(snapshot->bids.size() == 1);
    REQUIRE(snapshot->bids[0].level_price == price{100});
    REQUIRE(snapshot->bids[0].total_size == 5);
    REQUIRE(snapshot->asks.empty());

    runner.stop();
}

TEST_CASE("engine runner stop drains queued commands", "[pipeline]") {
    engine_runner runner{16};
    runner.start();

    auto first = runner.submit(submit_command{make_limit_order(1, 10, "ETH-USD", side::buy, 100, 1)});
    auto second = runner.submit(submit_command{make_limit_order(1, 11, "ETH-USD", side::buy, 101, 1)});

    runner.stop();

    require_event<submit_accepted>(get_ready(first), 0);
    require_event<submit_accepted>(get_ready(second), 0);
    REQUIRE_FALSE(runner.running());
}

TEST_CASE("engine runner rejects commands after stop", "[pipeline]") {
    engine_runner runner{8};
    runner.start();
    runner.stop();

    REQUIRE_THROWS_AS(
        runner.submit(submit_command{make_limit_order(1, 10, "ETH-USD", side::buy, 100, 1)}),
        std::runtime_error
    );
}
