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

The synchronous `processor` owns that sequencing state. It accepts fully
decoded packets and invokes a caller-supplied consumer only for packets ready
for ordered delivery. Multicast reception, application logging and recovery
decisions remain outside the processor.

`top_of_book_view` remains unavailable until a separately obtained snapshot
establishes both sides for a symbol. It applies later ordered multicast updates
and hides retained state after an application invalidates it. The snapshot is
deliberately absent from the multicast packet variant, keeping recovery traffic
out of the incremental hot path. Its future request/response transport and the
coordinator that rebases buffered incrementals are not implemented yet.

It depends on `matching`, `market_data_protocol` and ObzLib transport. Its
public API is in `obz::market_lab::market_data`. This component is the adapter
between the matching domain and the independent wire protocol.

The feed benchmarks deliberately exclude socket I/O. They measure decoding in
isolation and the synchronous decode, sequence and consume path under a long
in-order stream and fixed-size bursts. This provides a repeatable in-memory
baseline before considering another thread or queue. Tracy instrumentation in
the running receiver separately identifies time spent in blocking receive,
decode, sequencing and consumer work.
