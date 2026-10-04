# Matching

The matching capability contains two separately buildable modules:

- `engine` provides deterministic, single-threaded order-book and matching logic.
- `runtime` owns an engine on a worker thread and exchanges commands and results
  across an ObzLib bounded blocking queue.

Both modules expose APIs in `obz::market_lab::matching`. Keeping their CMake
targets separate allows code to use the domain engine without taking a runtime
dependency on queues or threads.
