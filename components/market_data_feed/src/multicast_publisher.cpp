#include <obz/market_lab/market_data/multicast_publisher.hpp>

#include <obz/market_lab/market_data/codec.hpp>

#include <obz/transport/udp_socket.hpp>

#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>

namespace obz::market_lab::market_data {

using namespace obz::market_lab::matching;

multicast_publisher::multicast_publisher(multicast_destination destination)
    : destination_{std::move(destination.group), destination.port} {
    socket_.open();
}

publish_result multicast_publisher::publish(std::span<const event> events) {
    publish_result result;

    for (const auto& emitted : events) {
        const auto* update = std::get_if<book_updated>(&emitted);
        if (update == nullptr) {
            continue;
        }

        if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error{"market-data sequence is exhausted"};
        }

        const auto bytes = encode(packet{next_sequence_, *update});
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
