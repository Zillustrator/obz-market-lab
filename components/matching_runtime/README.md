# Matching Runtime

The matching-runtime component runs a matching engine on a dedicated worker
thread. Commands cross that concurrency boundary through an ObzLib bounded
blocking queue and return results through futures.

It depends on `matching` but does not know how commands enter the process or
where emitted events are sent. Its public API is in
`obz::market_lab::matching_runtime`.
