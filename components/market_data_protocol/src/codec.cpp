#include <obz/market_lab/market_data/codec.hpp>

#include <obz/endian.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace obz::market_lab::market_data {

using namespace obz::market_lab::matching;
namespace {

namespace header_layout {

constexpr std::uint32_t signature{0x4f425a4d}; // "OBZM"
constexpr std::size_t size{16};

} // namespace header_layout

namespace book_update_layout {

constexpr std::size_t fixed_size{20};
constexpr std::uint8_t has_price_flag{0x01};

} // namespace book_update_layout

struct decoded_header {
    std::uint16_t encoded_message_type{};
    std::uint64_t sequence{};
};

using header_decode_result = std::variant<decoded_header, decode_error>;

template <typename T>
void write_and_advance(std::span<std::byte>& destination, T value) {
    obz::endian::write_be<T>(destination, 0, value);
    destination = destination.subspan(sizeof(T));
}

template <typename T>
T read_and_advance(std::span<const std::byte>& source) {
    const auto value = obz::endian::read_be<T>(source, 0);
    source = source.subspan(sizeof(T));
    return value;
}

void encode_header(
    std::span<std::byte> destination,
    message_type type,
    std::uint64_t sequence
) {
    write_and_advance(destination, header_layout::signature);
    write_and_advance(destination, protocol_version);
    write_and_advance(destination, static_cast<std::uint16_t>(type));
    write_and_advance(destination, sequence);
}

header_decode_result decode_header(std::span<const std::byte> bytes) {
    if (bytes.size() < header_layout::size) {
        return decode_error::header_truncated;
    }

    auto source = bytes.first(header_layout::size);

    if (read_and_advance<std::uint32_t>(source) != header_layout::signature) {
        return decode_error::invalid_signature;
    }

    if (read_and_advance<std::uint16_t>(source) != protocol_version) {
        return decode_error::unsupported_version;
    }

    const auto encoded_message_type = read_and_advance<std::uint16_t>(source);
    const auto sequence = read_and_advance<std::uint64_t>(source);
    if (sequence == 0) {
        return decode_error::invalid_sequence;
    }

    return decoded_header{encoded_message_type, sequence};
}

std::vector<std::byte> encode_book_update(
    const book_updated& update,
    std::uint64_t sequence
) {
    if (update.instrument.empty()) {
        throw std::invalid_argument("book update symbol must not be empty");
    }

    if (update.instrument.value.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("book update symbol is too long");
    }

    std::uint8_t encoded_side{};
    switch (update.direction) {
    case side::buy:
        encoded_side = 0;
        break;
    case side::sell:
        encoded_side = 1;
        break;
    default:
        throw std::invalid_argument("book update has an invalid side");
    }

    if (update.best_price.has_value() != (update.total_size > 0)) {
        throw std::invalid_argument(
            "book update must pair a price with positive aggregate size");
    }

    const auto packet_size =
        header_layout::size + book_update_layout::fixed_size +
        update.instrument.value.size();
    std::vector<std::byte> bytes(packet_size);
    encode_header(bytes, message_type::book_updated, sequence);

    auto destination = std::span<std::byte>{bytes}.subspan(header_layout::size);
    write_and_advance(destination, encoded_side);
    write_and_advance(
        destination,
        update.best_price.has_value() ? book_update_layout::has_price_flag : std::uint8_t{0});
    write_and_advance(
        destination,
        update.best_price.has_value() ? update.best_price->value : std::uint64_t{0});
    write_and_advance(destination, update.total_size);
    write_and_advance(
        destination,
        static_cast<std::uint16_t>(update.instrument.value.size()));

    std::transform(
        update.instrument.value.begin(),
        update.instrument.value.end(),
        destination.begin(),
        [](unsigned char character) { return static_cast<std::byte>(character); });

    return bytes;
}

decode_result decode_book_update(
    std::span<const std::byte> body,
    std::uint64_t sequence
) {
    if (body.size() < book_update_layout::fixed_size) {
        return decode_error::body_truncated;
    }

    auto source = body;
    const auto encoded_side = read_and_advance<std::uint8_t>(source);
    side decoded_side{};
    switch (encoded_side) {
    case 0:
        decoded_side = side::buy;
        break;
    case 1:
        decoded_side = side::sell;
        break;
    default:
        return decode_error::invalid_side;
    }

    const auto encoded_flags = read_and_advance<std::uint8_t>(source);
    if ((encoded_flags & ~book_update_layout::has_price_flag) != 0) {
        return decode_error::invalid_flags;
    }
    const auto has_price =
        (encoded_flags & book_update_layout::has_price_flag) != 0;

    const auto encoded_price = read_and_advance<std::uint64_t>(source);
    if (has_price != (encoded_price > 0)) {
        return decode_error::invalid_book_state;
    }

    std::optional<price> best_price;
    if (has_price) {
        best_price.emplace(encoded_price);
    }

    const auto total_size = read_and_advance<std::uint64_t>(source);
    if (has_price != (total_size > 0)) {
        return decode_error::invalid_book_state;
    }

    const auto symbol_length = read_and_advance<std::uint16_t>(source);
    if (symbol_length == 0) {
        return decode_error::invalid_symbol;
    }

    if (symbol_length != source.size()) {
        return decode_error::body_length_mismatch;
    }

    std::string instrument;
    instrument.reserve(symbol_length);
    for (const auto value : source) {
        instrument.push_back(static_cast<char>(std::to_integer<unsigned char>(value)));
    }

    return packet{
        sequence,
        book_updated{symbol{std::move(instrument)}, decoded_side, best_price, total_size}
    };
}

} // namespace

std::vector<std::byte> encode(const packet& value) {
    if (value.sequence == 0) {
        throw std::invalid_argument("market-data sequence must be positive");
    }

    return std::visit(
        [&](const auto& payload) {
            using payload_type = std::decay_t<decltype(payload)>;
            static_assert(std::is_same_v<payload_type, book_updated>);
            return encode_book_update(payload, value.sequence);
        },
        value.payload);
}

decode_result decode(std::span<const std::byte> bytes) {
    const auto header_result = decode_header(bytes);
    if (const auto* error = std::get_if<decode_error>(&header_result)) {
        return *error;
    }

    const auto& header = std::get<decoded_header>(header_result);
    const auto body = bytes.subspan(header_layout::size);

    switch (header.encoded_message_type) {
    case static_cast<std::uint16_t>(message_type::book_updated):
        return decode_book_update(body, header.sequence);
    default:
        return decode_error::unknown_message_type;
    }
}

std::string_view to_string(decode_error error) noexcept {
    switch (error) {
    case decode_error::header_truncated:
        return "header_truncated";
    case decode_error::invalid_signature:
        return "invalid_signature";
    case decode_error::unsupported_version:
        return "unsupported_version";
    case decode_error::unknown_message_type:
        return "unknown_message_type";
    case decode_error::invalid_sequence:
        return "invalid_sequence";
    case decode_error::body_truncated:
        return "body_truncated";
    case decode_error::body_length_mismatch:
        return "body_length_mismatch";
    case decode_error::invalid_symbol:
        return "invalid_symbol";
    case decode_error::invalid_side:
        return "invalid_side";
    case decode_error::invalid_flags:
        return "invalid_flags";
    case decode_error::invalid_book_state:
        return "invalid_book_state";
    }

    return "unknown_decode_error";
}

} // namespace obz::market_lab::market_data
