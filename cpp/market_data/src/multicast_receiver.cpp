#include <obz_market/market_data/multicast_receiver.hpp>

#include <obz/transport/udp_socket.hpp>

#include <span>
#include <utility>
#include <vector>

namespace obz_market::market_data {

struct multicast_receiver::implementation {
    static constexpr std::size_t maximum_udp_payload_size{65'507};

    obz::transport::udp_socket socket;
    std::vector<std::byte> receive_buffer{maximum_udp_payload_size};
};

multicast_receiver::multicast_receiver(multicast_subscription subscription)
    : impl_(std::make_unique<implementation>()) {
    impl_->socket.bind({"0.0.0.0", subscription.port});
    impl_->socket.join_multicast_group(
        subscription.group, subscription.interface_address);
}

multicast_receiver::~multicast_receiver() = default;

multicast_receive_result multicast_receiver::receive() {
    const auto received = impl_->socket.receive_from(impl_->receive_buffer);
    datagram_sender sender{received.sender.host, received.sender.port};

    if (received.status == obz::transport::datagram_status::truncated) {
        return truncated_packet{received.bytes_received, std::move(sender)};
    }

    const auto bytes = std::span<const std::byte>{impl_->receive_buffer}.first(
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

} // namespace obz_market::market_data
