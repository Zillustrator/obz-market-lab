# Obz Market Lab

Obz Market Lab is a learning and portfolio project for building, testing, and
profiling a small exchange-style order matching system in modern C++.

The project focuses on readable C++20, deterministic matching behaviour,
explicit performance tradeoffs, and practical tooling around tests, replay,
benchmarks, and profiling.

## Current Scope

- C++20 matching-engine core.
- Limit orders.
- Market orders.
- Cancellation.
- Order updates.
- Price-time priority.
- Multiple symbols.
- Deterministic event output.
- Focused tests for matching behaviour.
- Queue-backed engine runner that serialises commands onto an engine thread.
- Console replay demo that exercises the queued pipeline.

## Architecture

```text
scenario files / built-in demo
        |
        v
market_replay app
        |
        v
engine_runner  -- ObzLib bounded_blocking_queue -->  matching_engine
                                                    |
                                                    v
                                             per-symbol order_book
                                                    |
                                                    v
                                      events and book snapshots
```

The core `matching_engine` is deliberately single-threaded. It owns the active
order index and routes commands to one `order_book` per symbol. Each
`order_book` owns its bid and ask price levels, preserving price-time priority
with ordered price maps and FIFO queues at each level.

The `engine_runner` is the concurrency boundary. It serialises submit, update,
cancel, and snapshot commands through an ObzLib bounded blocking queue and
executes them on a dedicated engine thread.

## Matching Rules

The engine implements deterministic price-time priority across independent
per-symbol books. Limit orders may rest, market orders never rest,
cancellations remove active resting orders, and reduce-only order updates
preserve queue priority.

See `docs/matching-rules.md` for the full event ordering and update semantics.

## Build

Requirements:

- CMake with a C++20-capable compiler.

For local development beside the ObzLib checkout:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOBZ_MARKET_LAB_OBZ_SOURCE_DIR="/local/path/to/ObzLib/repo"

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

To consume the published ObzLib `v0.1.0` tag instead, omit
`OBZ_MARKET_LAB_OBZ_SOURCE_DIR`.

## VS Code

Shared VS Code tasks and launch configurations are included in `.vscode/`.

Use `Cmd + Shift + P` -> `Tasks: Run Task`, then search for `OML` to run
project tasks:

- `OML: Build Debug`
- `OML: Test All`
- `OML: Test Pipeline`
- `OML: Run Replay Demo`
- `OML: Run Replay Demo JSONL`
- `OML: Run Replay Scenarios`
- `OML: Benchmark Runner`
- `OML: Tracy Run Runner Benchmark`

Use the Run and Debug panel (`Cmd + Shift + D`) for launch configurations:

- `OML: Debug Tests All`
- `OML: Debug Tests Pipeline`
- `OML: Debug Replay Demo`
- `OML: Run Benchmarks Runner`
- `OML: Run Tracy Benchmark Runner`

The default VS Code build uses the tagged ObzLib dependency. It does not require
a local ObzLib checkout.

## Replay Demo

The replay demo is a small console application that drives the queue-backed
`engine_runner`, prints emitted events, and requests snapshots after key steps.
It can run a built-in walkthrough, or it can replay a text scenario file.

```bash
cmake --build build --target obz_market_replay --parallel
./build/apps/market_replay/obz_market_replay
```

Run without scenario source arguments to use the built-in walkthrough. Use
`--file` to replay one scenario, or `--directory` to replay every `.txt`
scenario file in that directory:

```bash
./build/apps/market_replay/obz_market_replay
./build/apps/market_replay/obz_market_replay --file path/to/custom-scenario.txt
./build/apps/market_replay/obz_market_replay --directory scenarios
```

Use `--format jsonl` to write one machine-readable JSON object per output
event or snapshot. The default format is `text`.

```bash
./build/apps/market_replay/obz_market_replay --directory scenarios --format jsonl
```

Included scenarios:

```text
scenarios/basic-cross.txt        Basic resting, crossing, cancellation, and market order flow.
scenarios/market-order.txt       Market orders consuming available liquidity.
scenarios/multi-level-cross.txt  Price-time priority across multiple price levels.
scenarios/cancel-reject.txt      Successful cancellation and rejected inactive cancels.
scenarios/multi-symbol.txt       Independent books for separate instruments.
scenarios/order-update.txt       Reduce-only updates and repricing that crosses the book.
```

Scenario files use a small whitespace-separated command format:

```text
submit limit  <user> <client_order> <instrument> <buy|sell> <price> <size>
submit market <user> <client_order> <instrument> <buy|sell> <size>
update limit  <user> <client_order> <price> <size>
cancel        <user> <client_order>
snapshot      <instrument> <depth>
```

The scenario parser is intentionally file-oriented: comments, blank lines, and
line-numbered parse errors are for editable replay files rather than live socket
protocol input.

In VS Code, run `OML: Run Replay Demo`, `OML: Run Replay Demo JSONL`, or
`OML: Run Replay Scenarios` from `Tasks: Run Task`. Use
`OML: Debug Replay Demo` from the Run and Debug panel.

## Benchmarking

Benchmarks are available through Google Benchmark, but are disabled by default.
Use a dedicated Release build when collecting timings:

```bash
cmake -S . -B build-bench-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DOBZ_MARKET_LAB_BUILD_TESTS=OFF \
  -DOBZ_MARKET_LAB_BUILD_BENCHMARKS=ON

cmake --build build-bench-release \
  --target obz_market_engine_benchmarks \
  --parallel

./build-bench-release/benchmarks/obz_market_engine_benchmarks
```

See `docs/benchmarking.md` for benchmark details and useful command-line
options.

## Pipeline

The matching engine remains a single-threaded domain object. The pipeline layer
adds an `engine_runner` that owns the engine on a worker thread and accepts
submit, cancel, and snapshot commands through an ObzLib bounded blocking queue.

## Profiling

Tracy instrumentation is available, but is disabled by default. Enable it in a
dedicated profiling build:

```bash
cmake -S . -B build-tracy-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DOBZ_MARKET_LAB_BUILD_TESTS=OFF \
  -DOBZ_MARKET_LAB_BUILD_BENCHMARKS=ON \
  -DOBZ_MARKET_LAB_ENABLE_TRACY=ON

cmake --build build-tracy-release \
  --target obz_market_engine_benchmarks \
  --parallel
```

Start the Tracy profiler:

```bash
/opt/homebrew/bin/tracy-profiler
```

Then run a benchmark long enough to connect:

```bash
./build-tracy-release/benchmarks/obz_market_engine_benchmarks \
  --benchmark_filter=crossing_limit_matches_price_levels/100 \
  --benchmark_min_time=120s
```

The Tracy profiler version must match the Tracy client version pinned in
`CMakeLists.txt`. See `docs/profiling.md` for the full profiling workflow.
