#pragma once

#include <obz_market/events.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace obz_market::market_data {

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
    ~multicast_publisher();

    multicast_publisher(const multicast_publisher&) = delete;
    multicast_publisher& operator=(const multicast_publisher&) = delete;
    multicast_publisher(multicast_publisher&&) = delete;
    multicast_publisher& operator=(multicast_publisher&&) = delete;

    publish_result publish(std::span<const event> events);

private:
    struct implementation;

    std::unique_ptr<implementation> impl_;
};

} // namespace obz_market::market_data
