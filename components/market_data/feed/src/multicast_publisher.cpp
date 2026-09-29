#include <obz/market_lab/market_data/multicast_publisher.hpp>

#include <obz/transport/udp_socket.hpp>

#include <stdexcept>
#include <utility>

namespace obz::market_lab::market_data {

multicast_publisher::multicast_publisher(multicast_destination destination)
    : destination_{std::move(destination.group), destination.port} {
    socket_.open();
}

publish_result multicast_publisher::publish(std::span<const matching::event> events) {
    publish_result result;

    for (const auto& emitted : events) {
        auto datagram = encoder_.encode(emitted);
        if (!datagram.has_value()) {
            continue;
        }

        const auto bytes_sent = socket_.send_to(destination_, datagram->bytes);
        if (bytes_sent != datagram->bytes.size()) {
            throw std::runtime_error{"UDP send did not consume the complete datagram"};
        }

        if (result.messages_published == 0) {
            result.first_sequence = datagram->sequence;
        }
        result.last_sequence = datagram->sequence;
        ++result.messages_published;
        result.bytes_sent += bytes_sent;
    }

    return result;
}

} // namespace obz::market_lab::market_data
