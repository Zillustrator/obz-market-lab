#include <obz/market_lab/market_data/multicast_receiver.hpp>
#include <obz/market_lab/market_data/processor.hpp>

#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
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

struct listener_options {
    std::string group{default_group};
    std::uint16_t port{default_port};
    std::string interface_address{default_interface};
    std::optional<std::size_t> stop_after;
    bool quiet{};
};

struct listener_counters {
    std::size_t datagrams{};
    std::size_t decoded{};
    std::size_t delivered{};
    std::size_t buffered{};
    std::size_t rejected{};
    std::size_t truncated{};
    std::size_t malformed{};
};

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

listener_options parse_options(int argc, char** argv) {
    listener_options options;
    std::size_t positional_index{};

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto next_value = [&](std::string_view name) -> std::string_view {
            if (++index >= argc) {
                throw std::invalid_argument{std::string{name} + " requires a value"};
            }
            return argv[index];
        };

        if (argument == "--stop-after") {
            options.stop_after = parse_positive_integer<std::size_t>(
                next_value(argument), "stop-after packet count");
        } else if (argument == "--quiet") {
            options.quiet = true;
        } else if (argument.starts_with('-')) {
            throw std::invalid_argument{"unknown option: " + std::string{argument}};
        } else if (positional_index == 0) {
            options.group = argument;
            ++positional_index;
        } else if (positional_index == 1) {
            options.port = parse_positive_integer<std::uint16_t>(argument, "port");
            ++positional_index;
        } else if (positional_index == 2) {
            options.interface_address = argument;
            ++positional_index;
        } else {
            throw std::invalid_argument{"too many positional arguments"};
        }
    }

    return options;
}

std::string_view side_name(side value) {
    return value == side::buy ? "buy" : "sell";
}

void print_packet(const packet& decoded) {
    const auto& update = std::get<book_update>(decoded.payload);

    std::cout << "received sequence=" << decoded.sequence
              << " symbol_id=" << update.instrument.value
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
        const auto options = parse_options(argc, argv);

        install_shutdown_handlers();
        multicast_receiver receiver{{
            options.group, options.port, options.interface_address}};
        std::cout << "listening group=" << options.group
                  << " port=" << options.port
                  << " interface=" << options.interface_address << std::endl;

        bool received_invalid_packet{};
        listener_counters counters;
        std::optional<std::chrono::steady_clock::time_point> first_datagram;
        std::optional<std::chrono::steady_clock::time_point> last_delivery;
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

            const auto received_at = std::chrono::steady_clock::now();
            if (!first_datagram.has_value()) {
                first_datagram = received_at;
            }
            ++counters.datagrams;

            if (const auto* truncated = std::get_if<truncated_packet>(&result)) {
                ++counters.truncated;
                ++counters.rejected;
                std::cerr << "discarded truncated datagram from "
                          << truncated->sender.address << ':' << truncated->sender.port
                          << " bytes_retained=" << truncated->bytes_retained << '\n';
                received_invalid_packet = true;
                continue;
            }

            if (const auto* malformed = std::get_if<malformed_packet>(&result)) {
                ++counters.malformed;
                ++counters.rejected;
                std::cerr << "discarded malformed packet from "
                          << malformed->sender.address << ':' << malformed->sender.port
                          << ": " << to_string(malformed->error) << '\n';
                received_invalid_packet = true;
                continue;
            }

            auto& received = std::get<received_packet>(result);
            ++counters.decoded;
            const auto sequence = received.value.sequence;
            const auto processed = packet_processor.process(
                std::move(received.value),
                [&](const packet& value) {
                    if (!options.quiet) {
                        print_packet(value);
                    }
                });
            counters.delivered += processed.packets_delivered;
            if (processed.packets_delivered != 0) {
                last_delivery = std::chrono::steady_clock::now();
            }

            if (processed.ordering == sequence_result::buffered) {
                ++counters.buffered;
            }

            if (processed.ordering != sequence_result::ready &&
                processed.ordering != sequence_result::buffered) {
                ++counters.rejected;
                print_processing_error(processed.ordering, sequence);
                received_invalid_packet = true;
            }

            if (options.stop_after.has_value() &&
                counters.delivered >= *options.stop_after) {
                break;
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

        std::int64_t elapsed_microseconds{};
        if (first_datagram.has_value() && last_delivery.has_value()) {
            elapsed_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                *last_delivery - *first_datagram).count();
        }
        const auto messages_per_second = elapsed_microseconds > 0
            ? static_cast<double>(counters.delivered) * 1'000'000.0 /
                static_cast<double>(elapsed_microseconds)
            : 0.0;

        std::cout << "listener summary datagrams=" << counters.datagrams
                  << " decoded=" << counters.decoded
                  << " delivered=" << counters.delivered
                  << " buffered=" << counters.buffered
                  << " rejected=" << counters.rejected
                  << " truncated=" << counters.truncated
                  << " malformed=" << counters.malformed
                  << " elapsed_us=" << elapsed_microseconds
                  << " messages_per_second=" << messages_per_second << '\n';
        std::cout << "listener stopped" << std::endl;

        return received_invalid_packet ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "receiver error: " << error.what() << '\n';
        std::cerr << "usage: obz_market_data_listener [group] [port] [interface] "
                     "[--stop-after packets] [--quiet]\n";
        return 1;
    }
}
