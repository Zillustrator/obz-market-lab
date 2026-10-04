#pragma once

#include <obz/market_lab/market_data/feed_encoder.hpp>

#include <obz/transport/udp_socket.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace obz::market_lab::market_data {

struct multicast_destination {
    std::string group;
    std::uint16_t port{};
};

struct publish_result {
    std::size_t messages_published{};
    std::size_t bytes_sent{};
    std::uint64_t first_sequence{};
    std::uint64_t last_sequence{};
};

class multicast_publisher {
public:
    explicit multicast_publisher(multicast_destination destination);

    multicast_publisher(const multicast_publisher&) = delete;
    multicast_publisher& operator=(const multicast_publisher&) = delete;

    publish_result publish(std::span<const matching::event> events);

private:
    feed_encoder encoder_;
    obz::transport::udp_socket socket_;
    obz::transport::endpoint destination_;
};

} // namespace obz::market_lab::market_data
