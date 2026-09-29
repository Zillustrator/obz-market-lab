# Market Data Protocol

The market-data protocol component defines the versioned packet model and its
network-byte-order codec. It validates bytes without opening sockets, which
keeps malformed-packet behavior deterministic and easy to test.

Its public API is in `obz::market_lab::market_data`. The complete version 1 wire
contract is documented in `docs/market-data-protocol.md`.
