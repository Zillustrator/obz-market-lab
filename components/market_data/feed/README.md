# Market Data Feed

The market-data feed component connects matching events to UDP multicast. The
`feed_encoder` selects top-of-book events, translates them into protocol-owned
messages, assigns feed sequence numbers and encodes packets without opening a
socket. The `multicast_publisher` sends those encoded packets, one per datagram.
The receiver owns multicast group membership, reuses its receive buffer,
reports truncation and decodes complete datagrams. The socket-independent
`sequenced_buffer<T>` keeps a bounded set of packets that arrive ahead of the
next expected sequence and releases them in order when the gap closes. Its
deferred factory is invoked only when a value must be stored, leaving the
normal in-order value in caller-owned storage.

It depends on `matching`, `market_data_protocol` and ObzLib transport. Its
public API is in `obz::market_lab::market_data`. This component is the adapter
between the matching domain and the independent wire protocol.
