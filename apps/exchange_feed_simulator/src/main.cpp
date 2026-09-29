#include <obz/market_lab/market_data/multicast_publisher.hpp>
#include <obz/market_lab/matching_runtime.hpp>

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace obz::market_lab::matching;
using namespace obz::market_lab::matching_runtime;
using namespace obz::market_lab::market_data;

constexpr std::string_view default_group{"239.255.0.1"};
constexpr std::uint16_t default_port{30001};

std::uint16_t parse_port(std::string_view text) {
    std::uint32_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{} || end != text.data() + text.size() || value == 0 ||
        value > 65'535) {
        throw std::invalid_argument{"port must be an integer from 1 to 65535"};
    }

    return static_cast<std::uint16_t>(value);
}

submit_order make_limit_order(
    std::uint64_t user,
    std::uint64_t client_order,
    side direction,
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

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 3) {
            throw std::invalid_argument{"usage: obz_market_exchange_feed_simulator [group] [port]"};
        }

        const auto group = argc >= 2 ? std::string{argv[1]} : std::string{default_group};
        const auto port = argc == 3 ? parse_port(argv[2]) : default_port;

        multicast_publisher publisher{{group, port}};
        engine_runner engine{64};
        engine.start();

        publish_events(
            publisher,
            engine.submit(submit_command{make_limit_order(1, 1, side::buy, 100, 10)}).get());
        publish_events(
            publisher,
            engine.submit(submit_command{make_limit_order(2, 1, side::sell, 101, 8)}).get());
        publish_events(
            publisher,
            engine.cancel(cancel_command{cancel_order{
                order_key{user_id{1}, client_order_id{1}}
            }}).get());

        engine.stop();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "publisher error: " << error.what() << '\n';
        return 1;
    }
}
