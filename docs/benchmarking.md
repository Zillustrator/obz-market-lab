# Benchmarking

Obz Market Lab uses Google Benchmark for focused matching, matching-runtime and
market-data feed benchmarks.

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
  --target obz_market_lab_matching_benchmarks \
           obz_market_lab_matching_runtime_benchmarks \
           obz_market_lab_market_data_feed_benchmarks \
  --parallel
```

Run the benchmark executable directly:

```bash
./build-bench-release/components/matching/engine/benchmarks/obz_market_lab_matching_benchmarks
./build-bench-release/components/matching/runtime/benchmarks/obz_market_lab_matching_runtime_benchmarks
./build-bench-release/components/market_data/feed/benchmarks/obz_market_lab_market_data_feed_benchmarks
```

Useful Google Benchmark options:

```bash
./build-bench-release/components/matching/engine/benchmarks/obz_market_lab_matching_benchmarks --benchmark_min_time=1s
./build-bench-release/components/matching/engine/benchmarks/obz_market_lab_matching_benchmarks --benchmark_filter=cancel
./build-bench-release/components/matching/engine/benchmarks/obz_market_lab_matching_benchmarks --benchmark_format=json
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
- market-data book-update decoding
- sustained synchronous decode, sequence, and consume processing
- synchronous bursts of 16, 64, and 256 market-data packets

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
./build-bench-release/components/matching/engine/benchmarks/obz_market_lab_matching_benchmarks \
  --benchmark_min_time=0.1s

./build-bench-release/components/matching/runtime/benchmarks/obz_market_lab_matching_runtime_benchmarks \
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

### Market-data synchronous baseline

These additional results were collected on the same class of local machine on
2026-10-04 using a Release build and Google Benchmark `v1.9.1`:

```bash
./build-bench-release/components/market_data/feed/benchmarks/obz_market_lab_market_data_feed_benchmarks \
  --benchmark_min_time=0.1s
```

The benchmark excludes socket I/O and console output. Processor construction
and the preparation of encoded input are outside the timed sections.

| Benchmark | CPU Time | Throughput |
| --- | ---: | ---: |
| `decode_book_update` | 12.4 ns/packet | 80.9 M packets/s |
| `synchronous_in_order` | 16.5 ns/packet | 60.5 M packets/s |
| `synchronous_burst/16` | 661 ns/burst | 24.2 M packets/s |
| `synchronous_burst/64` | 1.34 us/burst | 47.8 M packets/s |
| `synchronous_burst/256` | 4.10 us/burst | 62.4 M packets/s |

These numbers establish a local comparison baseline. They do not describe
network throughput because receiving, kernel scheduling, logging and downstream
application work are absent.

### Numeric symbol-ID comparison

Replacing the owning symbol string with a strongly typed 32-bit ID reduced the
following in-memory representations on the Apple Silicon Clang/libc++ build:

| Type | String symbol | Numeric symbol ID |
| --- | ---: | ---: |
| Matching symbol value | 24 B | 4 B |
| `submit_order` | 72 B | 48 B |
| `book_updated` | 56 B | 32 B |
| Matching `event` variant | 80 B | 64 B |
| Protocol `book_update` | 56 B | 32 B |
| Protocol `message` variant | 64 B | 40 B |
| Protocol `packet` | 72 B | 48 B |

The version 2 encoded top-of-book packet is a fixed 38 bytes, compared with 43
bytes for the version 1 packet containing the seven-character `ETH-USD` symbol.
C++ object sizes are ABI and platform dependent; wire sizes are protocol
contracts.

On 2026-10-05, both revision `5576f5e` and the symbol-ID working tree were
built in Release mode with AppleClang 21 and Google Benchmark 1.9.1 on the same
Mac. Each case ran three repetitions with `--benchmark_min_time=0.1s`; the table
uses median CPU time. Lower is better.

| Benchmark | String symbol | Numeric symbol ID | Change |
| --- | ---: | ---: | ---: |
| `decode_book_update` | 12.36 ns | 4.45 ns | 64% faster |
| `synchronous_in_order` | 16.45 ns | 6.94 ns | 58% faster |
| `synchronous_burst/64` | 1,395 ns | 738 ns | 47% faster |
| `synchronous_burst/256` | 4,262 ns | 1,842 ns | 57% faster |
| `resting_limit_submit` | 100.47 ns | 87.75 ns | 13% faster |
| `one_level_match` | 647.16 ns | 628.78 ns | 3% faster |
| `runner_submit_round_trip` | 2,452.50 ns | 2,375.87 ns | 3% faster |
| `runner_cancel_round_trip` | 2,993.71 ns | 3,061.07 ns | 2% slower |

The feed cases measure decoding and synchronous processing of prepared packets,
without socket I/O or console output. The runner cases are dominated by thread
scheduling and queue handoff, so their small differences are not evidence of a
meaningful change. The matching cases show smaller improvements than the feed
cases. Repeated runs under controlled machine load would give tighter timing
estimates.
