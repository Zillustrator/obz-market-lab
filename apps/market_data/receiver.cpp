#include <obz_market/market_data/multicast_receiver.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace {

using namespace obz_market;
using namespace obz_market::market_data;

constexpr std::string_view default_group{"239.255.0.1"};
constexpr std::uint16_t default_port{30001};
constexpr std::string_view default_interface{"0.0.0.0"};
constexpr std::size_t default_packet_count{3};

template <typename T>
T parse_positive_integer(std::string_view text, std::string_view name) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
        throw std::invalid_argument{std::string{name} + " must be a positive integer"};
    }

    return value;
}

std::string_view side_name(side value) {
    return value == side::buy ? "buy" : "sell";
}

void print_packet(const packet& decoded) {
    const auto& update = std::get<book_updated>(decoded.payload);

    std::cout << "received sequence=" << decoded.sequence
              << " symbol=" << update.instrument.value
              << " side=" << side_name(update.direction)
              << " best_price=";

    if (update.best_price) {
        std::cout << update.best_price->value;
    } else {
        std::cout << "none";
    }

    std::cout << " total_size=" << update.total_size << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 5) {
            throw std::invalid_argument{
                "usage: obz_market_data_receiver [group] [port] [interface] [count]"};
        }

        const auto group = argc >= 2 ? std::string{argv[1]} : std::string{default_group};
        const auto port = argc >= 3
            ? parse_positive_integer<std::uint16_t>(argv[2], "port")
            : default_port;
        const auto interface_address = argc >= 4
            ? std::string{argv[3]}
            : std::string{default_interface};
        const auto packet_count = argc == 5
            ? parse_positive_integer<std::size_t>(argv[4], "count")
            : default_packet_count;

        multicast_receiver receiver{{group, port, interface_address}};
        std::cout << "listening group=" << group
                  << " port=" << port
                  << " interface=" << interface_address
                  << " count=" << packet_count << std::endl;

        bool received_invalid_packet{};

        for (std::size_t index = 0; index < packet_count; ++index) {
            auto result = receiver.receive();

            if (const auto* truncated = std::get_if<truncated_packet>(&result)) {
                std::cerr << "discarded truncated datagram from "
                          << truncated->sender.address << ':' << truncated->sender.port
                          << " bytes_retained=" << truncated->bytes_retained << '\n';
                received_invalid_packet = true;
                continue;
            }

            if (const auto* malformed = std::get_if<malformed_packet>(&result)) {
                std::cerr << "discarded malformed packet from "
                          << malformed->sender.address << ':' << malformed->sender.port
                          << ": " << to_string(malformed->error) << '\n';
                received_invalid_packet = true;
                continue;
            }

            auto& received = std::get<received_packet>(result);

            // Exercise: track the expected feed sequence here. Decide how the receiver
            // should report a gap, duplicate or reordered packet without confusing that
            // stream policy with byte decoding or UDP transport errors.

            print_packet(received.value);
        }

        return received_invalid_packet ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "receiver error: " << error.what() << '\n';
        return 1;
    }
}
