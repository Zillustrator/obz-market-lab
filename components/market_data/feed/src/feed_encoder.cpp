#include <obz/market_lab/market_data/feed_encoder.hpp>

#include <obz/market_lab/market_data/codec.hpp>

#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace obz::market_lab::market_data {

namespace {

book_update to_book_update(const matching::book_updated& update) {
    side direction{};
    switch (update.direction) {
    case matching::side::buy:
        direction = side::buy;
        break;
    case matching::side::sell:
        direction = side::sell;
        break;
    default:
        throw std::invalid_argument{"matching update has an invalid side"};
    }

    std::optional<std::uint64_t> best_price;
    if (update.best_price.has_value()) {
        best_price = update.best_price->value;
    }

    return book_update{
        update.instrument.value,
        direction,
        best_price,
        update.total_size
    };
}

} // namespace

feed_encoder::feed_encoder(std::uint64_t first_sequence)
    : next_sequence_{first_sequence} {
    if (first_sequence == 0) {
        throw std::invalid_argument{"first market-data sequence must be positive"};
    }
}

std::optional<encoded_datagram> feed_encoder::encode(const matching::event& emitted) {
    const auto* update = std::get_if<matching::book_updated>(&emitted);
    if (update == nullptr) {
        return std::nullopt;
    }

    if (sequence_exhausted_) {
        throw std::overflow_error{"market-data sequence is exhausted"};
    }

    const auto sequence = next_sequence_;
    auto bytes = market_data::encode(packet{sequence, to_book_update(*update)});

    if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        sequence_exhausted_ = true;
    } else {
        ++next_sequence_;
    }

    return encoded_datagram{sequence, std::move(bytes)};
}

} // namespace obz::market_lab::market_data
