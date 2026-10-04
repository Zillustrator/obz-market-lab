#include <obz/market_lab/market_data/multicast_receiver.hpp>
#include <obz/market_lab/market_data/processor.hpp>

#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

#if !defined(_WIN32)
#include <signal.h>
#endif

namespace {

using namespace obz::market_lab::market_data;

constexpr std::string_view default_group{"239.255.0.1"};
constexpr std::uint16_t default_port{30001};
constexpr std::string_view default_interface{"0.0.0.0"};
constexpr std::size_t maximum_pending_packets{64};
constexpr std::uint64_t maximum_sequence_gap{64};

volatile std::sig_atomic_t shutdown_signal_received{};

void handle_shutdown_signal(int) {
    shutdown_signal_received = 1;
}

void install_shutdown_handlers() {
#if defined(_WIN32)
    if (std::signal(SIGINT, handle_shutdown_signal) == SIG_ERR ||
        std::signal(SIGTERM, handle_shutdown_signal) == SIG_ERR) {
        throw std::runtime_error{"failed to install shutdown signal handlers"};
    }
#else
    struct sigaction action {};
    action.sa_handler = handle_shutdown_signal;
    sigemptyset(&action.sa_mask);

    if (::sigaction(SIGINT, &action, nullptr) != 0 ||
        ::sigaction(SIGTERM, &action, nullptr) != 0) {
        throw std::system_error{
            errno,
            std::generic_category(),
            "failed to install shutdown signal handlers"};
    }
#endif
}

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
    const auto& update = std::get<book_update>(decoded.payload);

    std::cout << "received sequence=" << decoded.sequence
              << " symbol=" << update.symbol
              << " side=" << side_name(update.direction)
              << " best_price=";

    if (update.best_price) {
        std::cout << *update.best_price;
    } else {
        std::cout << "none";
    }

    std::cout << " aggregate_size=" << update.aggregate_size << std::endl;
}

void print_processing_error(sequence_result result, std::uint64_t sequence) {
    switch (result) {
    case sequence_result::old:
        std::cerr << "discarded old packet sequence=" << sequence << '\n';
        break;
    case sequence_result::duplicate:
        std::cerr << "discarded duplicate pending packet sequence=" << sequence << '\n';
        break;
    case sequence_result::gap_limit_exceeded:
        std::cerr << "sequence gap limit exceeded at sequence=" << sequence << '\n';
        break;
    case sequence_result::capacity_exhausted:
        std::cerr << "sequence buffer capacity exhausted at sequence=" << sequence << '\n';
        break;
    case sequence_result::ready:
    case sequence_result::buffered:
        break;
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 4) {
            throw std::invalid_argument{
                "usage: obz_market_data_listener [group] [port] [interface]"};
        }

        const auto group = argc >= 2 ? std::string{argv[1]} : std::string{default_group};
        const auto port = argc >= 3
            ? parse_positive_integer<std::uint16_t>(argv[2], "port")
            : default_port;
        const auto interface_address = argc >= 4
            ? std::string{argv[3]}
            : std::string{default_interface};

        install_shutdown_handlers();
        multicast_receiver receiver{{group, port, interface_address}};
        std::cout << "listening group=" << group
                  << " port=" << port
                  << " interface=" << interface_address << std::endl;

        bool received_invalid_packet{};
        processor packet_processor{
            processor_config{
                .sequence_limits = sequenced_buffer_limits{
                    .maximum_pending_values = maximum_pending_packets,
                    .maximum_sequence_gap = maximum_sequence_gap
                }
            }
        };

        while (shutdown_signal_received == 0) {
            multicast_receive_result result;
            try {
                result = receiver.receive();
            } catch (const std::system_error& error) {
                if (shutdown_signal_received != 0 &&
                    error.code() == std::errc::interrupted) {
                    break;
                }
                throw;
            }

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
            const auto sequence = received.value.sequence;
            const auto processed = packet_processor.process(
                std::move(received.value),
                [](const packet& value) {
                    print_packet(value);
                });

            if (processed.ordering != sequence_result::ready &&
                processed.ordering != sequence_result::buffered) {
                print_processing_error(processed.ordering, sequence);
                received_invalid_packet = true;
            }
        }

        if (packet_processor.pending_size() != 0) {
            std::cerr << "unresolved sequence gap";
            if (const auto next = packet_processor.next_sequence()) {
                std::cerr << ": expected=" << *next;
            }
            std::cerr << " pending=" << packet_processor.pending_size() << '\n';
            received_invalid_packet = true;
        }

        std::cout << "listener stopped" << std::endl;

        return received_invalid_packet ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "receiver error: " << error.what() << '\n';
        return 1;
    }
}
