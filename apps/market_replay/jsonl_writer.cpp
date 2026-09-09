#include "jsonl_writer.hpp"

#include <ostream>
#include <string_view>
#include <variant>

namespace obz_market::replay {

namespace {

constexpr char hex_digit(unsigned value) {
    return static_cast<char>(value < 10 ? '0' + value : 'a' + (value - 10));
}

std::string_view to_string(side direction) {
    switch (direction) {
    case side::buy:
        return "buy";
    case side::sell:
        return "sell";
    }

    return "unknown";
}

void write_json_string(std::ostream& output, std::string_view text) {
    output << '"';

    for (const char value : text) {
        switch (value) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(value) < 0x20) {
                const auto byte = static_cast<unsigned char>(value);
                output << "\\u00" << hex_digit(byte >> 4) << hex_digit(byte & 0x0f);
            } else {
                output << value;
            }
            break;
        }
    }

    output << '"';
}

void write_price(std::ostream& output, const std::optional<price>& value) {
    if (value.has_value()) {
        output << value->value;
    } else {
        output << "null";
    }
}

} // namespace

jsonl_writer::jsonl_writer(std::ostream& output)
    : output_(output) {}

void jsonl_writer::write_events(const std::vector<event>& events) {
    for (const auto& emitted : events) {
        write_event(emitted);
    }
}

void jsonl_writer::write_snapshot(const std::optional<book_snapshot>& snapshot) {
    output_ << "{\"type\":\"snapshot\",\"snapshot\":";

    if (!snapshot.has_value()) {
        output_ << "null}\n";
        return;
    }

    output_ << "{\"instrument\":";
    write_json_string(output_, snapshot->instrument.value);
    output_ << ",\"bids\":";
    write_price_levels(snapshot->bids);
    output_ << ",\"asks\":";
    write_price_levels(snapshot->asks);
    output_ << "}}\n";
}

void jsonl_writer::write_event(const event& emitted) {
    std::visit(
        [this](const auto& value) {
            write_event(value);
        },
        emitted
    );
}

void jsonl_writer::write_event(const submit_accepted& value) {
    output_ << "{\"type\":\"submit_accepted\",\"key\":";
    write_key(value.key);
    output_ << "}\n";
}

void jsonl_writer::write_event(const submit_rejected& value) {
    output_ << "{\"type\":\"submit_rejected\",\"key\":";
    write_key(value.key);
    output_ << ",\"reason\":";
    write_json_string(output_, value.reason);
    output_ << "}\n";
}

void jsonl_writer::write_event(const update_accepted& value) {
    output_ << "{\"type\":\"update_accepted\",\"key\":";
    write_key(value.key);
    output_ << "}\n";
}

void jsonl_writer::write_event(const update_rejected& value) {
    output_ << "{\"type\":\"update_rejected\",\"key\":";
    write_key(value.key);
    output_ << ",\"reason\":";
    write_json_string(output_, value.reason);
    output_ << "}\n";
}

void jsonl_writer::write_event(const cancel_rejected& value) {
    output_ << "{\"type\":\"cancel_rejected\",\"key\":";
    write_key(value.key);
    output_ << ",\"reason\":";
    write_json_string(output_, value.reason);
    output_ << "}\n";
}

void jsonl_writer::write_event(const order_cancelled& value) {
    output_ << "{\"type\":\"order_cancelled\",\"key\":";
    write_key(value.key);
    output_ << "}\n";
}

void jsonl_writer::write_event(const trade_executed& value) {
    output_ << "{\"type\":\"trade_executed\",\"aggressor\":";
    write_key(value.aggressor);
    output_ << ",\"resting\":";
    write_key(value.resting);
    output_ << ",\"instrument\":";
    write_json_string(output_, value.instrument.value);
    output_ << ",\"price\":" << value.execution_price.value
            << ",\"size\":" << value.executed_size.value << "}\n";
}

void jsonl_writer::write_event(const book_updated& value) {
    output_ << "{\"type\":\"book_updated\",\"instrument\":";
    write_json_string(output_, value.instrument.value);
    output_ << ",\"side\":";
    write_json_string(output_, to_string(value.direction));
    output_ << ",\"best_price\":";
    write_price(output_, value.best_price);
    output_ << ",\"total_size\":" << value.total_size << "}\n";
}

void jsonl_writer::write_price_levels(const std::vector<price_level_snapshot>& levels) {
    output_ << '[';

    for (std::size_t index = 0; index < levels.size(); ++index) {
        if (index != 0) {
            output_ << ',';
        }

        output_ << "{\"price\":" << levels[index].level_price.value
                << ",\"total_size\":" << levels[index].total_size << '}';
    }

    output_ << ']';
}

void jsonl_writer::write_key(const order_key& key) {
    output_ << "{\"user\":" << key.user.value
            << ",\"client_order\":" << key.client_order.value << '}';
}

} // namespace obz_market::replay
