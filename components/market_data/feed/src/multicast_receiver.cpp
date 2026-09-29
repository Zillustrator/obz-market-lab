#include <obz/market_lab/market_data/multicast_receiver.hpp>

#include <obz/transport/udp_socket.hpp>

#include <span>
#include <utility>

namespace obz::market_lab::market_data {

multicast_receiver::multicast_receiver(multicast_subscription subscription) {
    socket_.bind({"0.0.0.0", subscription.port});
    socket_.join_multicast_group(
        subscription.group, subscription.interface_address);
}

multicast_receive_result multicast_receiver::receive() {
    const auto received = socket_.receive_from(receive_buffer_);
    datagram_sender sender{received.sender.host, received.sender.port};

    if (received.status == obz::transport::datagram_status::truncated) {
        return truncated_packet{received.bytes_received, std::move(sender)};
    }

    const auto bytes = std::span<const std::byte>{receive_buffer_}.first(
        received.bytes_received);
    auto decoded = decode(bytes);

    if (auto* error = std::get_if<decode_error>(&decoded)) {
        return malformed_packet{*error, std::move(sender)};
    }

    return received_packet{
        std::move(std::get<packet>(decoded)),
        std::move(sender)
    };
}

} // namespace obz::market_lab::market_data
