#include <obz/market_lab/market_data/multicast_publisher.hpp>

#include <obz/market_lab/market_data/codec.hpp>

#include <obz/transport/udp_socket.hpp>

#include <cstdint>
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

multicast_publisher::multicast_publisher(multicast_destination destination)
    : destination_{std::move(destination.group), destination.port} {
    socket_.open();
}

publish_result multicast_publisher::publish(std::span<const matching::event> events) {
    publish_result result;

    for (const auto& emitted : events) {
        const auto* update = std::get_if<matching::book_updated>(&emitted);
        if (update == nullptr) {
            continue;
        }

        if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error{"market-data sequence is exhausted"};
        }

        const auto bytes = encode(packet{next_sequence_, to_book_update(*update)});
        const auto bytes_sent = socket_.send_to(destination_, bytes);
        if (bytes_sent != bytes.size()) {
            throw std::runtime_error{"UDP send did not consume the complete datagram"};
        }

        if (result.messages_published == 0) {
            result.first_sequence = next_sequence_;
        }
        result.last_sequence = next_sequence_;
        ++next_sequence_;
        ++result.messages_published;
        result.bytes_sent += bytes_sent;
    }

    return result;
}

} // namespace obz::market_lab::market_data
