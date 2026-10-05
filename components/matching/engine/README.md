# Matching

The matching component contains the deterministic, single-threaded order-book
and matching logic. It owns order validation, price-time priority, active-order
tracking, trades, top-of-book events and snapshots.

Orders and books use a strongly typed 32-bit `symbol_id`. Resolving display
names such as `ETH-USD` is a reference-data concern outside the engine.

It does not own threads, queues, sockets or wire encoding. Its public API is in
`obz::market_lab::matching` and its unit tests and focused benchmarks live
beside the implementation.
