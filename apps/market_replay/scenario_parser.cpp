#include "scenario_parser.hpp"

#include <charconv>
#include <cctype>
#include <cstdint>
#include <format>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace obz_market::replay {

namespace {

std::string_view next_token(std::string_view& line) {
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
        line.remove_prefix(1);
    }

    const auto token_end = line.find_first_of(" \t\r\n");
    if (token_end == std::string_view::npos) {
        const auto token = line;
        line = {};
        return token;
    }

    const auto token = line.substr(0, token_end);
    line.remove_prefix(token_end + 1);
    return token;
}

side parse_side(std::string_view text, std::size_t line_number) {
    if (text == "buy") {
        return side::buy;
    }

    if (text == "sell") {
        return side::sell;
    }

    throw std::runtime_error{
        std::format("line {}: expected side to be 'buy' or 'sell'", line_number)
    };
}

std::uint64_t parse_uint64(
    std::string_view text,
    std::string_view field,
    std::size_t line_number
) {
    std::uint64_t value{};
    const auto* const begin = text.data();
    const auto* const end = text.data() + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);

    if (error != std::errc{} || position != end) {
        throw std::runtime_error{
            std::format(
                "line {}: expected unsigned integer for {}",
                line_number,
                field
            )
        };
    }

    return value;
}

std::uint64_t parse_positive_uint64(
    std::string_view text,
    std::string_view field,
    std::size_t line_number
) {
    const auto value = parse_uint64(text, field, line_number);

    if (value == 0) {
        throw std::runtime_error{
            std::format("line {}: {} must be positive", line_number, field)
        };
    }

    return value;
}

std::size_t parse_size(
    std::string_view text,
    std::string_view field,
    std::size_t line_number
) {
    const auto value = parse_uint64(text, field, line_number);

    if (value > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error{
            std::format(
                "line {}: {} is too large for this platform",
                line_number,
                field
            )
        };
    }

    return static_cast<std::size_t>(value);
}

std::string_view read_token(
    std::string_view& line,
    std::string_view field,
    std::size_t line_number
) {
    const auto token = next_token(line);
    if (token.empty()) {
        throw std::runtime_error{
            std::format("line {}: missing {}", line_number, field)
        };
    }

    return token;
}

void reject_extra_tokens(std::string_view& line, std::size_t line_number) {
    if (const auto extra = next_token(line); !extra.empty()) {
        throw std::runtime_error{
            std::format("line {}: unexpected token '{}'", line_number, extra)
        };
    }
}

std::string remove_comment(std::string line) {
    if (const auto comment = line.find('#'); comment != std::string::npos) {
        line.erase(comment);
    }

    return line;
}

submit_order make_limit_order(
    std::uint64_t user,
    std::uint64_t client_order,
    std::string instrument,
    side direction,
    std::uint64_t limit_price,
    std::uint64_t size
) {
    return submit_order{
        order_key{user_id{user}, client_order_id{client_order}},
        symbol{std::move(instrument)},
        direction,
        order_pricing{std::in_place_type<limit_order>, price{limit_price}},
        quantity{size}
    };
}

submit_order make_market_order(
    std::uint64_t user,
    std::uint64_t client_order,
    std::string instrument,
    side direction,
    std::uint64_t size
) {
    return submit_order{
        order_key{user_id{user}, client_order_id{client_order}},
        symbol{std::move(instrument)},
        direction,
        order_pricing{std::in_place_type<market_order>},
        quantity{size}
    };
}

cancel_order make_cancel_order(std::uint64_t user, std::uint64_t client_order) {
    return cancel_order{
        order_key{user_id{user}, client_order_id{client_order}}
    };
}

update_order make_limit_update(
    std::uint64_t user,
    std::uint64_t client_order,
    std::uint64_t limit_price,
    std::uint64_t size
) {
    return update_order{
        order_key{user_id{user}, client_order_id{client_order}},
        order_pricing{std::in_place_type<limit_order>, price{limit_price}},
        quantity{size}
    };
}

scenario_command parse_submit(std::string_view line, std::size_t line_number) {
    const auto pricing = read_token(line, "pricing", line_number);
    const auto user = parse_uint64(
        read_token(line, "user", line_number),
        "user",
        line_number
    );
    const auto client_order = parse_uint64(
        read_token(line, "client_order", line_number),
        "client_order",
        line_number
    );
    const auto instrument = read_token(line, "instrument", line_number);
    const auto direction = parse_side(read_token(line, "side", line_number), line_number);

    if (pricing == "limit") {
        const auto limit_price = parse_positive_uint64(
            read_token(line, "limit_price", line_number),
            "limit_price",
            line_number
        );
        const auto size = parse_positive_uint64(
            read_token(line, "size", line_number),
            "size",
            line_number
        );
        reject_extra_tokens(line, line_number);

        return scenario_submit{
            make_limit_order(
                user,
                client_order,
                std::string{instrument},
                direction,
                limit_price,
                size
            )
        };
    }

    if (pricing == "market") {
        const auto size = parse_positive_uint64(
            read_token(line, "size", line_number),
            "size",
            line_number
        );
        reject_extra_tokens(line, line_number);

        return scenario_submit{
            make_market_order(
                user,
                client_order,
                std::string{instrument},
                direction,
                size
            )
        };
    }

    throw std::runtime_error{
        std::format("line {}: expected pricing to be 'limit' or 'market'", line_number)
    };
}

scenario_command parse_cancel(std::string_view line, std::size_t line_number) {
    const auto user = parse_uint64(
        read_token(line, "user", line_number),
        "user",
        line_number
    );
    const auto client_order = parse_uint64(
        read_token(line, "client_order", line_number),
        "client_order",
        line_number
    );
    reject_extra_tokens(line, line_number);

    return scenario_cancel{make_cancel_order(user, client_order)};
}

scenario_command parse_update(std::string_view line, std::size_t line_number) {
    const auto pricing = read_token(line, "pricing", line_number);
    const auto user = parse_uint64(
        read_token(line, "user", line_number),
        "user",
        line_number
    );
    const auto client_order = parse_uint64(
        read_token(line, "client_order", line_number),
        "client_order",
        line_number
    );

    if (pricing == "limit") {
        const auto limit_price = parse_positive_uint64(
            read_token(line, "limit_price", line_number),
            "limit_price",
            line_number
        );
        const auto size = parse_positive_uint64(
            read_token(line, "size", line_number),
            "size",
            line_number
        );
        reject_extra_tokens(line, line_number);

        return scenario_update{
            make_limit_update(user, client_order, limit_price, size)
        };
    }

    throw std::runtime_error{
        std::format("line {}: expected update pricing to be 'limit'", line_number)
    };
}

scenario_command parse_snapshot(std::string_view line, std::size_t line_number) {
    const auto instrument = read_token(line, "instrument", line_number);
    const auto depth = parse_size(
        read_token(line, "depth", line_number),
        "depth",
        line_number
    );
    reject_extra_tokens(line, line_number);

    return scenario_snapshot{
        snapshot_command{symbol{std::string{instrument}}, depth}
    };
}

} // namespace

scenario_command scenario_parser::parse_command(
    std::string_view line,
    std::size_t line_number
) {
    auto remaining = line;
    const auto command = read_token(remaining, "command", line_number);

    if (command == "submit") {
        return parse_submit(remaining, line_number);
    }

    if (command == "update") {
        return parse_update(remaining, line_number);
    }

    if (command == "cancel") {
        return parse_cancel(remaining, line_number);
    }

    if (command == "snapshot") {
        return parse_snapshot(remaining, line_number);
    }

    throw std::runtime_error{
        std::format(
            "line {}: expected command to be 'submit', 'update', 'cancel', or 'snapshot'",
            line_number
        )
    };
}

std::vector<scenario_step> scenario_parser::load(std::string_view path) const {
    std::ifstream input{std::string{path}};
    if (!input) {
        throw std::runtime_error{
            std::format("failed to open scenario file: {}", path)
        };
    }

    std::vector<scenario_step> steps;
    std::string line;
    std::size_t line_number{};

    while (std::getline(input, line)) {
        ++line_number;

        auto command_text = remove_comment(line);
        auto probe = std::string_view{command_text};
        if (next_token(probe).empty()) {
            continue;
        }

        auto command = parse_command(command_text, line_number);
        steps.push_back(
            scenario_step{
                line_number,
                std::move(command_text),
                std::move(command)
            }
        );
    }

    return steps;
}

} // namespace obz_market::replay
