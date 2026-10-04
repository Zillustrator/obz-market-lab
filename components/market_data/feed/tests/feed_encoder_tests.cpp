#include <obz/market_lab/market_data/codec.hpp>
#include <obz/market_lab/market_data/feed_encoder.hpp>
#include <obz/market_lab/matching/events.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace {

namespace market_data = obz::market_lab::market_data;
namespace matching = obz::market_lab::matching;

market_data::packet decode_datagram(const market_data::encoded_datagram& datagram) {
    const auto decoded = market_data::decode(datagram.bytes);
    const auto* packet = std::get_if<market_data::packet>(&decoded);
    REQUIRE(packet != nullptr);
    return *packet;
}

const market_data::book_update& require_book_update(const market_data::packet& packet) {
    const auto* update = std::get_if<market_data::book_update>(&packet.payload);
    REQUIRE(update != nullptr);
    return *update;
}

matching::event priced_update(matching::side direction) {
    return matching::book_updated{
        matching::symbol{"ETH-USD"},
        direction,
        matching::price{101},
        7
    };
}

} // namespace

TEST_CASE("feed encoder ignores non-market-data events without consuming a sequence", "[market_data][feed]") {
    market_data::feed_encoder encoder{42};

    const matching::event accepted = matching::submit_accepted{};
    REQUIRE_FALSE(encoder.encode(accepted).has_value());

    const auto encoded = encoder.encode(priced_update(matching::side::buy));
    REQUIRE(encoded.has_value());
    REQUIRE(encoded->sequence == 42);
    REQUIRE(decode_datagram(*encoded).sequence == 42);
}

TEST_CASE("feed encoder translates matching book updates and assigns consecutive sequences", "[market_data][feed]") {
    market_data::feed_encoder encoder;

    const auto priced = encoder.encode(priced_update(matching::side::sell));
    REQUIRE(priced.has_value());

    const auto& priced_packet = decode_datagram(*priced);
    const auto& priced_body = require_book_update(priced_packet);
    REQUIRE(priced_packet.sequence == 1);
    REQUIRE(priced_body.symbol == "ETH-USD");
    REQUIRE(priced_body.direction == market_data::side::sell);
    REQUIRE(priced_body.best_price == std::uint64_t{101});
    REQUIRE(priced_body.aggregate_size == 7);

    const matching::event empty = matching::book_updated{
        matching::symbol{"ETH-USD"},
        matching::side::buy,
        std::nullopt,
        0
    };
    const auto encoded_empty = encoder.encode(empty);
    REQUIRE(encoded_empty.has_value());

    const auto& empty_packet = decode_datagram(*encoded_empty);
    const auto& empty_body = require_book_update(empty_packet);
    REQUIRE(empty_packet.sequence == 2);
    REQUIRE(empty_body.direction == market_data::side::buy);
    REQUIRE_FALSE(empty_body.best_price.has_value());
    REQUIRE(empty_body.aggregate_size == 0);
}

TEST_CASE("feed encoder requires a positive initial sequence", "[market_data][feed]") {
    REQUIRE_THROWS_AS(market_data::feed_encoder{0}, std::invalid_argument);
}

TEST_CASE("feed encoder uses the maximum sequence once before reporting exhaustion", "[market_data][feed]") {
    constexpr auto maximum_sequence = std::numeric_limits<std::uint64_t>::max();
    market_data::feed_encoder encoder{maximum_sequence};

    const auto encoded = encoder.encode(priced_update(matching::side::buy));
    REQUIRE(encoded.has_value());
    REQUIRE(encoded->sequence == maximum_sequence);
    REQUIRE(decode_datagram(*encoded).sequence == maximum_sequence);

    REQUIRE_THROWS_AS(
        encoder.encode(priced_update(matching::side::buy)),
        std::overflow_error);
}
