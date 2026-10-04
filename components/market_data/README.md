# Market Data

The market-data capability contains two separately buildable modules:

- `protocol` defines and validates the versioned wire representation.
- `feed` publishes and receives protocol packets over UDP multicast.

Both modules expose APIs in `obz::market_lab::market_data`. The feed depends on
the protocol, while the protocol remains independent of socket transport.
