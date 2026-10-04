# Market Data Protocol

The first Market Lab feed message carries one top-of-book update. The format is
deliberately small, versioned, and independent of C++ object layout. Every
multi-byte integer uses network byte order (big endian).

The protocol owns this message model. Matching-engine events are translated at
the feed boundary, so engine-specific types and event definitions are not part
of the wire contract.

## Packet structure

Every packet contains the same fixed-size header followed by one
message-specific body:

```text
+------------------------+---------------------------+
| Common header (16 B)   | Message-specific body     |
+------------------------+---------------------------+
```

The message type in the common header determines how the remaining bytes are
decoded.

## Common header

| Offset | Width | Field | Version 1 value |
|---:|---:|---|---|
| 0 | 4 | Protocol signature | ASCII `OBZM` |
| 4 | 2 | Protocol version | `1` |
| 6 | 2 | Message type | `1` for `book_updated` |
| 8 | 8 | Feed sequence | Positive, monotonically increasing publisher sequence |

One UDP datagram contains exactly one Market Lab packet. UDP preserves that
boundary, so the received span supplies the packet length. A malformed datagram
is discarded in full; the next receive begins at the next datagram.

## `book_updated` body

Body offsets start at zero, immediately after the common header. The equivalent
packet offset is the body offset plus 16 bytes.

| Body offset | Width | Field | Version 1 value |
|---:|---:|---|---|
| 0 | 1 | Side | `0` buy, `1` sell |
| 1 | 1 | Flags | Bit 0 means best price is present |
| 2 | 8 | Best price | Positive when present; zero when absent |
| 10 | 8 | Aggregate size | Positive when price is present; zero when absent |
| 18 | 2 | Symbol length | Positive number of following symbol bytes |
| 20 | variable | Symbol | Exactly `symbol length` bytes |

## Canonical book state

A populated side has both a positive best price and positive aggregate size. An
empty side has no best price and zero aggregate size. Other combinations are
rejected by both the local encoder and the untrusted-input decoder.

The feed sequence belongs to the publication stream, not to an individual
order or the matching engine's internal order-priority sequence. The codec
preserves the value without maintaining stream state. Feed processing may hold
a bounded number of packets received ahead of the next expected sequence and
release them if the gap closes. Exceeding the configured gap or capacity limit
requires an application recovery decision such as requesting retransmission or
obtaining a fresh snapshot.

## Error handling

Encoding invalid local values throws `std::invalid_argument`, which indicates a
programming or upstream-domain error. Decoding malformed network input returns
a `decode_error`; malformed traffic is expected input and does not use
exceptions for control flow.

Version 1 currently supports only `book_updated`. New message types can extend
the payload variant while retaining the common header. An incompatible wire
change requires a new protocol version.
