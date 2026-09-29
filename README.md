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
- Versioned binary encoding and validation for top-of-book market-data updates.
- Focused tests for matching behaviour.
- Queue-backed matching runner that serialises commands onto an engine thread.
- Console replay demo that exercises the matching runtime.

## Architecture

```text
scenario files / built-in demo
        |
        v
market_replay app
        |
        v
runner  -- ObzLib bounded_blocking_queue -->  engine
                                                    |
                                                    v
                                             per-symbol order_book
                                                    |
                                                    v
                                      events and book snapshots
```

The core `engine` is deliberately single-threaded. It owns the active
order index and routes commands to one `order_book` per symbol. Each
`order_book` owns its bid and ask price levels, preserving price-time priority
with ordered price maps and FIFO queues at each level.

The `runner` is the concurrency boundary. It serialises submit, update,
cancel, and snapshot commands through an ObzLib bounded blocking queue and
executes them on a dedicated engine thread.

The market-data protocol owns wire-facing messages and converts them to and
from an explicit network byte layout. It does not depend on the matching
component. The feed component translates matching events into protocol
messages and handles socket transport, leaving malformed-packet behavior easy
to test deterministically. See `docs/market-data-protocol.md` for the current
wire contract.

The multicast demo runs a publisher and receiver as separate processes. The
publisher submits commands through the queue-backed matching runner, selects the
resulting top-of-book events, assigns feed sequence numbers and sends one
encoded packet per UDP datagram. The receiver joins the configured group,
validates and decodes each datagram, then prints the update.

Production code is organised as self-contained components. Each component owns
its public headers, implementation, unit tests and focused benchmarks:

```text
components/
  matching/
    engine/    Single-threaded order-book and matching logic
    runtime/   Queue-backed threaded execution
  market_data/
    protocol/  Versioned packet encoding and decoding
    feed/      Multicast publishing and receiving
  tracing/     Shared optional instrumentation
```

Public C++ APIs use the `obz::market_lab` namespace and include paths beginning
with `obz/market_lab`.

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

To consume the pinned ObzLib revision instead, omit
`OBZ_MARKET_LAB_OBZ_SOURCE_DIR`. The pinned revision includes the multicast
membership API used by the market-data receiver.

## VS Code

Shared VS Code tasks and launch configurations are included in `.vscode/`.

Use `Cmd + Shift + P` -> `Tasks: Run Task`, then search for `Obz Market Lab`
to run project tasks. Linux tasks include:

- `Obz Market Lab Linux: Build Development Image`
- `Obz Market Lab Linux: Open Shell`
- `Obz Market Lab Linux: Test GCC`
- `Obz Market Lab Linux: Test Clang`
- `Obz Market Lab Linux: Run Multicast Demo`

macOS tasks include:

- `Obz Market Lab macOS: Build Debug`
- `Obz Market Lab macOS: Test All`
- `Obz Market Lab macOS: Test Matching Runtime`
- `Obz Market Lab macOS: Run Replay Demo`
- `Obz Market Lab macOS: Run Replay Demo JSONL`
- `Obz Market Lab macOS: Run Replay Scenarios`
- `Obz Market Lab macOS: Benchmark Runner`
- `Obz Market Lab macOS: Tracy Run Runner Benchmark`

Use the Run and Debug panel (`Cmd + Shift + D`) for launch configurations:

- `Obz Market Lab macOS: Debug Matching Tests`
- `Obz Market Lab macOS: Debug Matching Runtime Tests`
- `Obz Market Lab macOS: Debug Market Data Protocol Tests`
- `Obz Market Lab macOS: Debug Replay Tests`
- `Obz Market Lab macOS: Debug Replay Demo`
- `Obz Market Lab macOS: Run Benchmarks Runner`
- `Obz Market Lab macOS: Run Tracy Benchmark Runner`

The default VS Code build uses the tagged ObzLib dependency. It does not require
a local ObzLib checkout.

## Replay Demo

The replay demo is a small console application that drives the queue-backed
`runner`, prints emitted events, and requests snapshots after key steps.
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

In VS Code, run `Obz Market Lab macOS: Run Replay Demo`,
`Obz Market Lab macOS: Run Replay Demo JSONL`, or
`Obz Market Lab macOS: Run Replay Scenarios` from `Tasks: Run Task`. Use
`Obz Market Lab macOS: Debug Replay Demo` from the Run and Debug panel.

## Multicast Demo

Start the receiver before the publisher. Both applications default to multicast
group `239.255.0.1` and UDP port `30001`:

```bash
./build/apps/market_data_listener/obz_market_data_listener
```

In a second terminal:

```bash
./build/apps/exchange_feed_simulator/obz_market_exchange_feed_simulator
```

The receiver accepts optional group, port, interface address and packet count:

```text
obz_market_data_listener [group] [port] [interface] [count]
```

The publisher accepts an optional group and port:

```text
obz_market_exchange_feed_simulator [group] [port]
```

Interface `0.0.0.0` asks the operating system to choose the receiving interface.
The Linux VS Code task `Obz Market Lab Linux: Run Multicast Demo` builds both
applications, starts them as separate processes in one container, waits for
receiver readiness and fails after ten seconds rather than blocking forever.

## Benchmarking

Benchmarks are available through Google Benchmark, but are disabled by default.
Use a dedicated Release build when collecting timings:

```bash
cmake -S . -B build-bench-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DOBZ_MARKET_LAB_BUILD_TESTS=OFF \
  -DOBZ_MARKET_LAB_BUILD_BENCHMARKS=ON

cmake --build build-bench-release \
  --target obz_market_lab_matching_benchmarks \
           obz_market_lab_matching_runtime_benchmarks \
  --parallel

./build-bench-release/components/matching/engine/benchmarks/obz_market_lab_matching_benchmarks
./build-bench-release/components/matching/runtime/benchmarks/obz_market_lab_matching_runtime_benchmarks
```

See `docs/benchmarking.md` for benchmark details and useful command-line
options.

## Matching Runtime

The matching engine remains a single-threaded domain object. The matching
runtime adds a `runner` that owns the engine on a worker thread and accepts
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
  --target obz_market_lab_matching_benchmarks \
  --parallel
```

Start the Tracy profiler:

```bash
/opt/homebrew/bin/tracy-profiler
```

Then run a benchmark long enough to connect:

```bash
./build-tracy-release/components/matching/engine/benchmarks/obz_market_lab_matching_benchmarks \
  --benchmark_filter=crossing_limit_matches_price_levels/100 \
  --benchmark_min_time=120s
```

The Tracy profiler version must match the Tracy client version pinned in
`CMakeLists.txt`. See `docs/profiling.md` for the full profiling workflow.
