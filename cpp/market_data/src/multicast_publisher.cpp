#include <obz_market/market_data/multicast_publisher.hpp>

#include <obz_market/market_data/codec.hpp>

#include <obz/transport/udp_socket.hpp>

#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>

namespace obz_market::market_data {

struct multicast_publisher::implementation {
    obz::transport::udp_socket socket;
    obz::transport::endpoint destination;
    std::uint64_t next_sequence{1};
};

multicast_publisher::multicast_publisher(multicast_destination destination)
    : impl_(std::make_unique<implementation>()) {
    impl_->destination = {std::move(destination.group), destination.port};
    impl_->socket.open();
}

multicast_publisher::~multicast_publisher() = default;

publish_result multicast_publisher::publish(std::span<const event> events) {
    publish_result result;

    for (const auto& emitted : events) {
        const auto* update = std::get_if<book_updated>(&emitted);
        if (update == nullptr) {
            continue;
        }

        if (impl_->next_sequence == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error{"market-data sequence is exhausted"};
        }

        const auto bytes = encode(packet{impl_->next_sequence, *update});
        const auto bytes_sent = impl_->socket.send_to(impl_->destination, bytes);
        if (bytes_sent != bytes.size()) {
            throw std::runtime_error{"UDP send did not consume the complete datagram"};
        }

        if (result.messages_published == 0) {
            result.first_sequence = impl_->next_sequence;
        }
        result.last_sequence = impl_->next_sequence;
        ++impl_->next_sequence;
        ++result.messages_published;
        result.bytes_sent += bytes_sent;
    }

    return result;
}

} // namespace obz_market::market_data
