#include <obz/market_lab/market_data/multicast_publisher.hpp>
#include <obz/market_lab/matching/runner.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace matching = obz::market_lab::matching;

using namespace obz::market_lab::matching;
using namespace obz::market_lab::market_data;

constexpr std::string_view default_group{"239.255.0.1"};
constexpr std::uint16_t default_port{30001};
constexpr std::size_t default_burst_size{64};

struct simulator_options {
    std::string group{default_group};
    std::uint16_t port{default_port};
    std::optional<std::size_t> message_count;
    std::size_t burst_size{default_burst_size};
    std::uint64_t interval_microseconds{};
    bool quiet{};
};

template <typename T>
T parse_integer(std::string_view text, std::string_view name, bool allow_zero = false) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{} || end != text.data() + text.size() ||
        (!allow_zero && value == 0)) {
        throw std::invalid_argument{
            std::string{name} + (allow_zero ? " must be a non-negative integer"
                                           : " must be a positive integer")};
    }

    return value;
}

std::uint16_t parse_port(std::string_view text) {
    const auto value = parse_integer<std::uint32_t>(text, "port");
    if (value > 65'535) {
        throw std::invalid_argument{"port must be an integer from 1 to 65535"};
    }
    return static_cast<std::uint16_t>(value);
}

simulator_options parse_options(int argc, char** argv) {
    simulator_options options;
    std::size_t positional_index{};

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto next_value = [&](std::string_view name) -> std::string_view {
            if (++index >= argc) {
                throw std::invalid_argument{std::string{name} + " requires a value"};
            }
            return argv[index];
        };

        if (argument == "--messages") {
            options.message_count = parse_integer<std::size_t>(
                next_value(argument), "message count");
        } else if (argument == "--burst-size") {
            options.burst_size = parse_integer<std::size_t>(
                next_value(argument), "burst size");
        } else if (argument == "--interval-us") {
            options.interval_microseconds = parse_integer<std::uint64_t>(
                next_value(argument), "interval", true);
        } else if (argument == "--quiet") {
            options.quiet = true;
        } else if (argument.starts_with('-')) {
            throw std::invalid_argument{"unknown option: " + std::string{argument}};
        } else if (positional_index == 0) {
            options.group = argument;
            ++positional_index;
        } else if (positional_index == 1) {
            options.port = parse_port(argument);
            ++positional_index;
        } else {
            throw std::invalid_argument{"too many positional arguments"};
        }
    }

    return options;
}

submit_order make_limit_order(
    std::uint64_t user,
    std::uint64_t client_order,
    matching::side direction,
    std::uint64_t limit_price,
    std::uint64_t size
) {
    return submit_order{
        order_key{user_id{user}, client_order_id{client_order}},
        symbol{"ETH-USD"},
        direction,
        order_pricing{std::in_place_type<limit_order>, price{limit_price}},
        quantity{size}
    };
}

void publish_events(multicast_publisher& publisher, const std::vector<event>& events) {
    const auto result = publisher.publish(events);
    if (result.messages_published == 0) {
        return;
    }

    std::cout << "published messages=" << result.messages_published
              << " sequences=" << result.first_sequence << '-' << result.last_sequence
              << " bytes=" << result.bytes_sent << '\n';
}

publish_result publish_load(
    multicast_publisher& publisher,
    runner& engine,
    const simulator_options& options
) {
    const auto message_count = *options.message_count;
    publish_result total;

    for (std::size_t first = 0; first < message_count; first += options.burst_size) {
        const auto count = std::min(options.burst_size, message_count - first);
        std::vector<event> events;
        events.reserve(count * 2);

        for (std::size_t offset = 0; offset < count; ++offset) {
            const auto client_order = static_cast<std::uint64_t>(first + offset + 1);
            auto emitted = engine.submit(submit_command{make_limit_order(
                1,
                client_order,
                matching::side::buy,
                100,
                1
            )}).get();
            events.insert(
                events.end(),
                std::make_move_iterator(emitted.begin()),
                std::make_move_iterator(emitted.end()));
        }

        const auto result = publisher.publish(events);
        if (total.messages_published == 0 && result.messages_published != 0) {
            total.first_sequence = result.first_sequence;
        }
        total.messages_published += result.messages_published;
        total.bytes_sent += result.bytes_sent;
        total.last_sequence = result.last_sequence;

        if (!options.quiet) {
            std::cout << "published burst messages=" << result.messages_published
                      << " sequences=" << result.first_sequence << '-'
                      << result.last_sequence << " bytes=" << result.bytes_sent << '\n';
        }

        if (first + count < message_count && options.interval_microseconds != 0) {
            std::this_thread::sleep_for(
                std::chrono::microseconds{options.interval_microseconds});
        }
    }

    return total;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);

        multicast_publisher publisher{{options.group, options.port}};
        runner engine{64};
        engine.start();

        if (options.message_count.has_value()) {
            const auto started = std::chrono::steady_clock::now();
            const auto result = publish_load(publisher, engine, options);
            const auto elapsed = std::chrono::steady_clock::now() - started;
            const auto elapsed_microseconds =
                std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
            const auto messages_per_second = elapsed_microseconds > 0
                ? static_cast<double>(result.messages_published) * 1'000'000.0 /
                    static_cast<double>(elapsed_microseconds)
                : 0.0;

            std::cout << "publisher summary messages=" << result.messages_published
                      << " sequences=" << result.first_sequence << '-'
                      << result.last_sequence
                      << " bytes=" << result.bytes_sent
                      << " elapsed_us=" << elapsed_microseconds
                      << " messages_per_second=" << messages_per_second
                      << " burst_size=" << options.burst_size
                      << " interval_us=" << options.interval_microseconds << '\n';
        } else {
            publish_events(
                publisher,
                engine.submit(
                    submit_command{make_limit_order(1, 1, matching::side::buy, 100, 10)}
                ).get());
            publish_events(
                publisher,
                engine.submit(
                    submit_command{make_limit_order(2, 1, matching::side::sell, 101, 8)}
                ).get());
            publish_events(
                publisher,
                engine.cancel(cancel_command{cancel_order{
                    order_key{user_id{1}, client_order_id{1}}
                }}).get());
        }

        engine.stop();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "publisher error: " << error.what() << '\n';
        std::cerr << "usage: obz_market_exchange_feed_simulator [group] [port] "
                     "[--messages count] [--burst-size count] "
                     "[--interval-us microseconds] [--quiet]\n";
        return 1;
    }
}
