# Market Data Feed

The market-data feed component connects matching events to UDP multicast. The
publisher selects top-of-book events, assigns feed sequence numbers, encodes
packets and sends one packet per datagram. The receiver owns multicast group
membership, reuses its receive buffer, reports truncation and decodes complete
datagrams.

It depends on `matching`, `market_data_protocol` and ObzLib transport. Its
public API is in `obz::market_lab::market_data`.
