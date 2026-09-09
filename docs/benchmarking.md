# Benchmarking

Obz Market Lab uses Google Benchmark for focused C++ engine benchmarks.

Benchmarks are disabled by default so normal development builds stay fast and do
not fetch benchmark dependencies unless requested.

## Release Benchmark Build

Use a dedicated Release build directory for benchmark runs:

```bash
/opt/homebrew/bin/cmake -S . -B build-bench-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DOBZ_MARKET_LAB_BUILD_TESTS=OFF \
  -DOBZ_MARKET_LAB_BUILD_BENCHMARKS=ON

/opt/homebrew/bin/cmake --build build-bench-release \
  --target obz_market_engine_benchmarks \
  --parallel
```

Run the benchmark executable directly:

```bash
./build-bench-release/benchmarks/obz_market_engine_benchmarks
```

Useful Google Benchmark options:

```bash
./build-bench-release/benchmarks/obz_market_engine_benchmarks --benchmark_min_time=1s
./build-bench-release/benchmarks/obz_market_engine_benchmarks --benchmark_filter=cancel
./build-bench-release/benchmarks/obz_market_engine_benchmarks --benchmark_format=json
```

## Current Coverage

The benchmark suite covers:

- resting limit order submission
- cancelling a resting limit order
- reducing a resting order size while preserving priority
- repricing a resting order
- market order submission against an empty book
- one-level limit order matching
- crossing limit orders over 10, 50, and 100 price levels
- snapshots at depths 1, 10, 50, and 100
- engine-runner submit, update, cancel, and snapshot round trips through the command
  queue

These are baseline measurements rather than final performance claims. Debug
build timings should only be used as smoke tests because compiler optimization
level materially changes the result.

## Reading Results

Prefer comparing benchmark results from the same machine, same build type, and
similar system load. The most useful numbers are relative changes between
commits, for example whether a data-structure change improves cancel throughput
without regressing matching or snapshot performance.

For profiling hot paths, use Tracy separately. Google Benchmark answers how fast
a workload is; Tracy helps explain where the time is spent.

## Representative Results

These results were collected from a local Release build on 2026-09-09 using
Google Benchmark `v1.9.1`.

Machine:

- MacBook Pro
- Apple M3 Pro
- 36 GB memory
- macOS 26.6.2

```bash
./build-bench-release/benchmarks/obz_market_engine_benchmarks \
  --benchmark_min_time=0.1s
```

Google Benchmark was unable to report the Apple Silicon CPU frequency through
`sysctl`, so treat these numbers as local reference measurements rather than
portable hardware claims.

| Benchmark | Real Time | CPU Time |
| --- | ---: | ---: |
| `market_order_against_empty_book` | 35.2 ns | 35.2 ns |
| `snapshot_seeded_book/1` | 49.4 ns | 49.4 ns |
| `snapshot_seeded_book/10` | 70.9 ns | 70.9 ns |
| `resting_limit_submit` | 106.6 ns | 106.6 ns |
| `snapshot_seeded_book/50` | 245.4 ns | 245.4 ns |
| `snapshot_seeded_book/100` | 469.9 ns | 469.9 ns |
| `reduce_resting_order_size` | 586.3 ns | 588.8 ns |
| `cancel_resting_limit_order` | 598.4 ns | 599.5 ns |
| `reprice_resting_order` | 646.0 ns | 645.8 ns |
| `one_level_match` | 650.4 ns | 652.3 ns |
| `crossing_limit_matches_price_levels/10` | 1.3 us | 1.3 us |
| `crossing_limit_matches_price_levels/50` | 3.3 us | 3.3 us |
| `crossing_limit_matches_price_levels/100` | 5.7 us | 5.7 us |
| `runner_cancel_round_trip` | 5.3 us | 3.0 us |
| `runner_update_round_trip` | 5.3 us | 3.1 us |
| `runner_submit_round_trip` | 4.5 us | 2.2 us |
| `runner_snapshot_round_trip` | 4.4 us | 2.3 us |
