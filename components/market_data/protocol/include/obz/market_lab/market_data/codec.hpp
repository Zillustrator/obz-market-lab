#pragma once

#include <obz/market_lab/market_data/messages.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace obz::market_lab::market_data {

inline constexpr std::uint16_t protocol_version{2};

enum class message_type : std::uint16_t {
    book_updated = 1
};

struct packet {
    std::uint64_t sequence{};
    message payload;
};

enum class decode_error {
    header_truncated,
    invalid_signature,
    unsupported_version,
    unknown_message_type,
    invalid_sequence,
    body_truncated,
    body_length_mismatch,
    invalid_symbol,
    invalid_side,
    invalid_flags,
    invalid_book_state
};

using decode_result = std::variant<packet, decode_error>;

std::vector<std::byte> encode(const packet& value);
decode_result decode(std::span<const std::byte> bytes);
std::string_view to_string(decode_error error) noexcept;

} // namespace obz::market_lab::market_data
