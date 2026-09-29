# Market Data Feed

The market-data feed component connects matching events to UDP multicast. The
publisher selects top-of-book events, translates them into protocol-owned
messages, assigns feed sequence numbers, encodes packets and sends one packet
per datagram. The receiver owns multicast group membership, reuses its receive
buffer, reports truncation and decodes complete datagrams.

It depends on `matching`, `market_data_protocol` and ObzLib transport. Its
public API is in `obz::market_lab::market_data`. This component is the adapter
between the matching domain and the independent wire protocol.
