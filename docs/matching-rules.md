# Matching Rules

This document describes the current deterministic matching behaviour of the
Obz Market Lab C++ engine.

The engine is intentionally small. These rules describe the implemented
behaviour, not a complete exchange specification.

## Core Concepts

The matching engine manages one independent order book per symbol.

Each submitted order has:

- an `order_key`, made from a user id and client order id
- a symbol
- a side: buy or sell
- pricing: limit or market
- a positive quantity

Each symbol book has:

- bid levels ordered from highest price to lowest price
- ask levels ordered from lowest price to highest price
- FIFO ordering within each price level

## Submit Validation

A submit command is rejected before reaching an order book when:

- the user id is zero
- the client order id is zero
- the symbol is empty
- the quantity is zero
- the order key is already active

Rejected submit commands emit:

```text
submit_rejected
```

Accepted submit commands emit:

```text
submit_accepted
```

## Order Updates

Only active resting limit orders can be updated.

An update command identifies the order by `order_key` and supplies a new limit
price and open quantity. The order's symbol and side are retained from the
active resting order.

Successful update commands emit:

```text
update_accepted
```

A reduce-only limit update at the same price preserves time priority. The
existing resting order stays in its price-level queue and only its open quantity
is changed.

An update that changes price or increases quantity loses time priority. The
existing resting order is removed, then the updated order is processed as the
aggressor with a new internal sequence number. Any unfilled quantity rests at
the new limit price.

Update commands are rejected when:

- the order key is not active
- the update uses market pricing

Rejected update commands emit:

```text
update_rejected
```

## Limit Orders

A buy limit order matches resting asks while the best ask price is less than or
equal to the incoming limit price.

A sell limit order matches resting bids while the best bid price is greater than
or equal to the incoming limit price.

If a limit order still has open quantity after matching, the remaining quantity
rests on its own side of the book.

## Market Orders

A market order matches the best available opposite-side liquidity until either:

- the incoming order is fully filled
- the opposite side of the book is empty

Market orders do not rest. Any unfilled market quantity expires after matching.

## Price-Time Priority

The engine applies price-time priority:

- buy orders consume the lowest ask first
- sell orders consume the highest bid first
- within the same price level, older resting orders fill before newer resting
  orders

Each resting order receives an internal sequence number when accepted by the
book. The current implementation preserves time priority by appending orders to
the back of each price-level queue and matching from the front.

## Trade Price

Trades execute at the resting order price.

For example, if a buy limit order at `105` matches a resting sell order at
`100`, the trade price is `100`.

Trade events emit:

```text
trade_executed
```

Each trade event records:

- aggressor order key
- resting order key
- symbol
- execution price
- executed size

## Book Updates

The engine emits book updates when the top of book changes for a side.

A book update contains:

- symbol
- side
- best price, or empty when the side has no remaining levels
- total size available at that best price

Book update events emit:

```text
book_updated
```

Only top-of-book changes emit `book_updated`. Changes away from the best price
are visible through snapshots, not through top-of-book events.

## Cancellation

Only active resting orders can be cancelled.

A successful cancel removes the order's remaining open quantity from its price
level. If the price level becomes empty, the level is removed from the book.

Successful cancel commands emit:

```text
order_cancelled
```

If the cancellation changes the top of book, a `book_updated` event is emitted
after `order_cancelled`.

Cancel commands are rejected when the order key is not active.

Rejected cancel commands emit:

```text
cancel_rejected
```

## Active Orders

The matching engine tracks active orders separately from each order book.

An order is active only while it has resting quantity in a book. Fully filled
orders, cancelled orders, and market orders with no resting quantity are not
active.

This active-order index is used to make cancellation lookup direct rather than
searching through all price levels.

## Snapshots

Snapshots provide a deterministic read-only view of one symbol book.

An order book snapshot contains:

- symbol
- bid levels
- ask levels

Each price-level snapshot contains:

- level price
- total size at that price

Bid levels are returned best first, from highest price to lowest price.

Ask levels are returned best first, from lowest price to highest price.

Snapshot depth limits bids and asks independently. A depth of zero returns empty
bid and ask vectors.

Requesting a snapshot for a missing symbol returns no snapshot.

## Event Ordering

For submit commands that reach the book, `submit_accepted` is emitted first.

Trade events are emitted in execution order.

Book updates are emitted after the matching or resting state change that caused
them.

For successful cancels, `order_cancelled` is emitted before any resulting
`book_updated` event.

For successful updates, `update_accepted` is emitted first. If the update
crosses the book, trade events are emitted after `update_accepted`, followed by
any resulting `book_updated` events.

## Current Limitations

The current engine does not implement:

- stop orders
- fill-or-kill orders
- immediate-or-cancel orders
- iceberg orders
- per-symbol trading status
- tick-size validation
- persistence
- concurrent command processing

These behaviours are intentionally outside the current scope.
