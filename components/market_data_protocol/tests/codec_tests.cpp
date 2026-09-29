#include <obz/market_lab/market_data/codec.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <variant>
#include <vector>

namespace {

using namespace obz::market_lab::matching;
using namespace obz::market_lab::market_data;

const packet& require_packet(const decode_result& result) {
    const auto* decoded = std::get_if<packet>(&result);
    REQUIRE(decoded != nullptr);
    return *decoded;
}

decode_error require_error(const decode_result& result) {
    const auto* error = std::get_if<decode_error>(&result);
    REQUIRE(error != nullptr);
    return *error;
}

packet priced_update() {
    return packet{
        42,
        book_updated{symbol{"ETH-USD"}, side::buy, price{101}, 7}
    };
}

void require_book_update(
    const packet& decoded,
    std::uint64_t sequence,
    const symbol& instrument,
    side direction,
    const std::optional<price>& best_price,
    std::uint64_t total_size
) {
    REQUIRE(decoded.sequence == sequence);

    const auto* update = std::get_if<book_updated>(&decoded.payload);
    REQUIRE(update != nullptr);
    REQUIRE(update->instrument == instrument);
    REQUIRE(update->direction == direction);
    REQUIRE(update->best_price == best_price);
    REQUIRE(update->total_size == total_size);
}

} // namespace

TEST_CASE("market-data codec writes a stable network-byte-order packet", "[market_data]") {
    const auto bytes = encode(priced_update());

    const std::vector<std::byte> expected{
        std::byte{0x4f}, std::byte{0x42}, std::byte{0x5a}, std::byte{0x4d},
        std::byte{0x00}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x2a},
        std::byte{0x00}, std::byte{0x01},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x65},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x07},
        std::byte{0x00}, std::byte{0x07},
        std::byte{0x45}, std::byte{0x54}, std::byte{0x48}, std::byte{0x2d},
        std::byte{0x55}, std::byte{0x53}, std::byte{0x44}
    };

    REQUIRE(bytes.size() == expected.size());
    REQUIRE(std::equal(bytes.begin(), bytes.end(), expected.begin(), expected.end()));
}

TEST_CASE("market-data codec round trips priced and empty book updates", "[market_data]") {
    const auto priced = priced_update();
    require_book_update(
        require_packet(decode(encode(priced))),
        42,
        symbol{"ETH-USD"},
        side::buy,
        price{101},
        7);

    const packet empty{
        43,
        book_updated{symbol{"ETH-USD"}, side::sell, std::nullopt, 0}
    };
    require_book_update(
        require_packet(decode(encode(empty))),
        43,
        symbol{"ETH-USD"},
        side::sell,
        std::nullopt,
        0);
}

TEST_CASE("market-data encoder rejects non-canonical packets", "[market_data]") {
    REQUIRE_THROWS_AS(
        encode(packet{0, book_updated{symbol{"ETH-USD"}, side::buy, price{100}, 2}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        encode(packet{1, book_updated{symbol{""}, side::buy, price{100}, 2}}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        encode(packet{1, book_updated{symbol{"ETH-USD"}, side::buy, price{100}, 0}}),
        std::invalid_argument);
}

TEST_CASE("market-data decoder reports malformed headers", "[market_data]") {
    const std::array<std::byte, 3> short_packet{};
    REQUIRE(require_error(decode(short_packet)) == decode_error::header_truncated);

    auto bytes = encode(priced_update());
    bytes[0] = std::byte{0};
    REQUIRE(require_error(decode(bytes)) == decode_error::invalid_signature);

    bytes = encode(priced_update());
    bytes[5] = std::byte{2};
    REQUIRE(require_error(decode(bytes)) == decode_error::unsupported_version);

    bytes = encode(priced_update());
    bytes[7] = std::byte{2};
    REQUIRE(require_error(decode(bytes)) == decode_error::unknown_message_type);

}

TEST_CASE("market-data decoder reports malformed book updates", "[market_data]") {
    auto bytes = encode(priced_update());
    bytes[8] = std::byte{0};
    bytes[15] = std::byte{0};
    REQUIRE(require_error(decode(bytes)) == decode_error::invalid_sequence);

    bytes = encode(priced_update());
    bytes[16] = std::byte{2};
    REQUIRE(require_error(decode(bytes)) == decode_error::invalid_side);

    bytes = encode(priced_update());
    bytes[17] = std::byte{0x80};
    REQUIRE(require_error(decode(bytes)) == decode_error::invalid_flags);

    bytes = encode(priced_update());
    bytes[17] = std::byte{0};
    REQUIRE(require_error(decode(bytes)) == decode_error::invalid_book_state);

    bytes = encode(priced_update());
    bytes.resize(35);
    REQUIRE(require_error(decode(bytes)) == decode_error::body_truncated);

    bytes = encode(priced_update());
    bytes[35] = std::byte{6};
    REQUIRE(require_error(decode(bytes)) == decode_error::body_length_mismatch);
}
