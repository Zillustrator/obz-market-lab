#include <obz_market/engine.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>
#include <utility>

namespace {

using namespace obz_market;

constexpr auto default_symbol = "ETH-USD";

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

void seed_ask_levels(
    matching_engine& engine,
    std::uint64_t first_client_order,
    std::uint64_t first_price,
    std::uint64_t levels
) {
    for (std::uint64_t index = 0; index < levels; ++index) {
        benchmark::DoNotOptimize(engine.submit(make_limit_order(
            1,
            first_client_order + index,
            default_symbol,
            side::sell,
            first_price + index,
            1
        )));
    }
}

void resting_limit_submit(benchmark::State& state) {
    matching_engine engine;
    std::uint64_t client_order = 1;

    for (auto _ : state) {
        benchmark::DoNotOptimize(engine.submit(make_limit_order(
            1,
            client_order++,
            default_symbol,
            side::buy,
            100,
            10
        )));
    }
}

void cancel_resting_limit_order(benchmark::State& state) {
    std::uint64_t client_order = 1;

    for (auto _ : state) {
        state.PauseTiming();
        matching_engine engine;
        const auto key = order_key{user_id{1}, client_order_id{client_order++}};
        benchmark::DoNotOptimize(engine.submit(submit_order{
            key,
            symbol{default_symbol},
            side::buy,
            order_pricing{std::in_place_type<limit_order>, price{100}},
            quantity{10}
        }));
        state.ResumeTiming();

        benchmark::DoNotOptimize(engine.cancel(cancel_order{key}));
    }
}

void reduce_resting_order_size(benchmark::State& state) {
    std::uint64_t client_order = 1;

    for (auto _ : state) {
        state.PauseTiming();
        matching_engine engine;
        const auto key = order_key{user_id{1}, client_order_id{client_order++}};
        benchmark::DoNotOptimize(engine.submit(submit_order{
            key,
            symbol{default_symbol},
            side::buy,
            order_pricing{std::in_place_type<limit_order>, price{100}},
            quantity{10}
        }));
        state.ResumeTiming();

        benchmark::DoNotOptimize(engine.update(update_order{
            key,
            order_pricing{std::in_place_type<limit_order>, price{100}},
            quantity{5}
        }));
    }
}

void reprice_resting_order(benchmark::State& state) {
    std::uint64_t client_order = 1;

    for (auto _ : state) {
        state.PauseTiming();
        matching_engine engine;
        const auto key = order_key{user_id{1}, client_order_id{client_order++}};
        benchmark::DoNotOptimize(engine.submit(submit_order{
            key,
            symbol{default_symbol},
            side::buy,
            order_pricing{std::in_place_type<limit_order>, price{100}},
            quantity{10}
        }));
        state.ResumeTiming();

        benchmark::DoNotOptimize(engine.update(update_order{
            key,
            order_pricing{std::in_place_type<limit_order>, price{101}},
            quantity{10}
        }));
    }
}

void market_order_against_empty_book(benchmark::State& state) {
    matching_engine engine;
    std::uint64_t client_order = 1;

    for (auto _ : state) {
        benchmark::DoNotOptimize(engine.submit(make_market_order(
            1,
            client_order++,
            default_symbol,
            side::buy,
            10
        )));
    }
}

void one_level_match(benchmark::State& state) {
    std::uint64_t client_order = 1;

    for (auto _ : state) {
        state.PauseTiming();
        matching_engine engine;
        benchmark::DoNotOptimize(engine.submit(make_limit_order(
            1,
            client_order++,
            default_symbol,
            side::sell,
            100,
            10
        )));
        state.ResumeTiming();

        benchmark::DoNotOptimize(engine.submit(make_limit_order(
            2,
            client_order++,
            default_symbol,
            side::buy,
            100,
            10
        )));
    }
}

void crossing_limit_matches_price_levels(benchmark::State& state) {
    const auto levels = static_cast<std::uint64_t>(state.range(0));
    std::uint64_t client_order = 1;

    for (auto _ : state) {
        state.PauseTiming();
        matching_engine engine;
        seed_ask_levels(engine, client_order, 100, levels);
        client_order += levels;
        state.ResumeTiming();

        benchmark::DoNotOptimize(engine.submit(make_limit_order(
            2,
            client_order++,
            default_symbol,
            side::buy,
            100 + levels - 1,
            levels
        )));
    }
}

void snapshot_seeded_book(benchmark::State& state) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    matching_engine engine;

    for (std::uint64_t index = 0; index < 100; ++index) {
        benchmark::DoNotOptimize(engine.submit(make_limit_order(
            1,
            index + 1,
            default_symbol,
            side::buy,
            100 - index,
            index + 1
        )));
        benchmark::DoNotOptimize(engine.submit(make_limit_order(
            2,
            index + 1,
            default_symbol,
            side::sell,
            101 + index,
            index + 1
        )));
    }

    for (auto _ : state) {
        benchmark::DoNotOptimize(engine.snapshot(symbol{default_symbol}, depth));
    }
}

BENCHMARK(resting_limit_submit);
BENCHMARK(cancel_resting_limit_order);
BENCHMARK(reduce_resting_order_size);
BENCHMARK(reprice_resting_order);
BENCHMARK(market_order_against_empty_book);
BENCHMARK(one_level_match);
BENCHMARK(crossing_limit_matches_price_levels)->Arg(10)->Arg(50)->Arg(100);
BENCHMARK(snapshot_seeded_book)->Arg(1)->Arg(10)->Arg(50)->Arg(100);

} // namespace
