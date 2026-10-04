# Profiling

Obz Market Lab has optional Tracy instrumentation for profiling C++ matching,
runtime and market-data paths.

Tracy is disabled by default. Enable it only in builds where profiling is
needed:

```bash
/opt/homebrew/bin/cmake -S . -B build-tracy-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DOBZ_MARKET_LAB_BUILD_TESTS=OFF \
  -DOBZ_MARKET_LAB_BUILD_BENCHMARKS=ON \
  -DOBZ_MARKET_LAB_BUILD_APPS=ON \
  -DOBZ_MARKET_LAB_ENABLE_TRACY=ON

/opt/homebrew/bin/cmake --build build-tracy-release \
  --target obz_market_lab_matching_benchmarks \
           obz_market_lab_matching_runtime_benchmarks \
           obz_market_data_listener \
  --parallel
```

Run a benchmark workload while the Tracy profiler is available:

```bash
./build-tracy-release/components/matching/engine/benchmarks/obz_market_lab_matching_benchmarks \
  --benchmark_filter=resting_limit_submit \
  --benchmark_min_time=5s
```

Use the matching-runtime executable when profiling queue and worker-thread
round trips:

```bash
./build-tracy-release/components/matching/runtime/benchmarks/obz_market_lab_matching_runtime_benchmarks \
  --benchmark_filter=runner_submit_round_trip \
  --benchmark_min_time=5s
```

To profile the socket-driven market-data path, start the Tracy-enabled listener
and then run the exchange feed simulator in another terminal:

```bash
./build-tracy-release/apps/market_data_listener/obz_market_data_listener
./build/apps/exchange_feed_simulator/obz_market_exchange_feed_simulator
```

The current simulator sends three updates. That is enough to verify zone names
and nesting in Tracy, but it is not a representative profiling workload or a
basis for performance conclusions. A repeatable sustained-load publisher is a
separate follow-up.

## Instrumented Areas

The current instrumentation covers:

- `engine::submit`
- `engine::update`
- `engine::cancel`
- `engine::snapshot`
- `order_book::submit`
- `order_book::update`
- `order_book::cancel`
- `order_book::snapshot`
- bid and ask submit/update/cancel paths
- matching against price levels
- resting order insertion
- snapshot level creation
- `runner` enqueue, run-loop, and command-processing zones
- multicast receive
- market-data decode
- packet sequencing and ordered delivery
- market-data consumer work

The zones are intentionally coarse. They should show which engine paths dominate
before adding more detailed instrumentation inside loops or data-structure
operations.
