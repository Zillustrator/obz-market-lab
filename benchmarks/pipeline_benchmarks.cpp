#include <obz_market/pipeline.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

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

void runner_submit_round_trip(benchmark::State& state) {
    engine_runner runner{1024};
    runner.start();

    std::uint64_t client_order = 1;

    for (auto _ : state) {
        auto events = runner.submit(submit_command{make_limit_order(
            1,
            client_order++,
            default_symbol,
            side::buy,
            100,
            10
        )});

        benchmark::DoNotOptimize(events.get());
    }

    runner.stop();
}

void runner_cancel_round_trip(benchmark::State& state) {
    engine_runner runner{1024};
    runner.start();

    std::uint64_t client_order = 1;

    for (auto _ : state) {
        state.PauseTiming();
        const auto key = order_key{user_id{1}, client_order_id{client_order++}};
        runner.submit(submit_command{submit_order{
            key,
            symbol{default_symbol},
            side::buy,
            order_pricing{std::in_place_type<limit_order>, price{100}},
            quantity{10}
        }}).get();
        state.ResumeTiming();

        auto events = runner.cancel(cancel_command{cancel_order{key}});
        benchmark::DoNotOptimize(events.get());
    }

    runner.stop();
}

void runner_update_round_trip(benchmark::State& state) {
    engine_runner runner{1024};
    runner.start();

    std::uint64_t client_order = 1;

    for (auto _ : state) {
        state.PauseTiming();
        const auto key = order_key{user_id{1}, client_order_id{client_order++}};
        runner.submit(submit_command{submit_order{
            key,
            symbol{default_symbol},
            side::buy,
            order_pricing{std::in_place_type<limit_order>, price{100}},
            quantity{10}
        }}).get();
        state.ResumeTiming();

        auto events = runner.update(update_command{update_order{
            key,
            order_pricing{std::in_place_type<limit_order>, price{100}},
            quantity{5}
        }});
        benchmark::DoNotOptimize(events.get());
    }

    runner.stop();
}

void runner_snapshot_round_trip(benchmark::State& state) {
    engine_runner runner{1024};
    runner.start();

    for (std::uint64_t index = 0; index < 100; ++index) {
        runner.submit(submit_command{make_limit_order(
            1,
            index + 1,
            default_symbol,
            side::buy,
            100 - index,
            index + 1
        )}).get();
    }

    for (auto _ : state) {
        auto snapshot = runner.snapshot(snapshot_command{symbol{default_symbol}, 10});
        benchmark::DoNotOptimize(snapshot.get());
    }

    runner.stop();
}

BENCHMARK(runner_submit_round_trip);
BENCHMARK(runner_cancel_round_trip);
BENCHMARK(runner_update_round_trip);
BENCHMARK(runner_snapshot_round_trip);

} // namespace
