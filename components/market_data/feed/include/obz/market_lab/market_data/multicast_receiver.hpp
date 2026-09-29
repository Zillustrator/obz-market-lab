#pragma once

#include <obz/market_lab/market_data/codec.hpp>

#include <obz/transport/udp_socket.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace obz::market_lab::market_data {

struct multicast_subscription {
    std::string group;
    std::uint16_t port{};
    std::string interface_address{"0.0.0.0"};
};

struct datagram_sender {
    std::string address;
    std::uint16_t port{};
};

struct received_packet {
    packet value;
    datagram_sender sender;
};

struct truncated_packet {
    std::size_t bytes_retained{};
    datagram_sender sender;
};

struct malformed_packet {
    decode_error error;
    datagram_sender sender;
};

using multicast_receive_result =
    std::variant<received_packet, truncated_packet, malformed_packet>;

class multicast_receiver {
public:
    explicit multicast_receiver(multicast_subscription subscription);

    multicast_receiver(const multicast_receiver&) = delete;
    multicast_receiver& operator=(const multicast_receiver&) = delete;

    multicast_receive_result receive();

private:
    static constexpr std::size_t maximum_udp_payload_size{65'507};

    obz::transport::udp_socket socket_;
    std::vector<std::byte> receive_buffer_{maximum_udp_payload_size};
};

} // namespace obz::market_lab::market_data
