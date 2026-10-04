# Market Data Protocol

The market-data protocol component defines the versioned packet model and its
network-byte-order codec. It validates bytes without opening sockets, which
keeps malformed-packet behavior deterministic and easy to test.

The protocol owns its wire-facing message types and has no dependency on the
matching component. A feed adapter translates matching events into protocol
messages before encoding them. This keeps engine-specific strong types and
business events out of the network contract.

Its public API is in `obz::market_lab::market_data`. The complete version 1 wire
contract is documented in `docs/market-data-protocol.md`.
