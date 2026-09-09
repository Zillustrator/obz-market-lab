# Profiling

Obz Market Lab has optional Tracy instrumentation for profiling C++ engine hot
paths.

Tracy is disabled by default. Enable it only in builds where profiling is
needed:

```bash
/opt/homebrew/bin/cmake -S . -B build-tracy-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DOBZ_MARKET_LAB_BUILD_TESTS=OFF \
  -DOBZ_MARKET_LAB_BUILD_BENCHMARKS=ON \
  -DOBZ_MARKET_LAB_ENABLE_TRACY=ON

/opt/homebrew/bin/cmake --build build-tracy-release \
  --target obz_market_engine_benchmarks \
  --parallel
```

Run a benchmark workload while the Tracy profiler is available:

```bash
./build-tracy-release/benchmarks/obz_market_engine_benchmarks \
  --benchmark_filter=resting_limit_submit \
  --benchmark_min_time=5s
```

## Instrumented Areas

The current instrumentation covers:

- `matching_engine::submit`
- `matching_engine::update`
- `matching_engine::cancel`
- `matching_engine::snapshot`
- `order_book::submit`
- `order_book::update`
- `order_book::cancel`
- `order_book::snapshot`
- bid and ask submit/update/cancel paths
- matching against price levels
- resting order insertion
- snapshot level creation
- `engine_runner` enqueue, run-loop, and command-processing zones

The zones are intentionally coarse. They should show which engine paths dominate
before adding more detailed instrumentation inside loops or data-structure
operations.
