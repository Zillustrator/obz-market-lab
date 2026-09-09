#include "console_writer.hpp"

#include <ostream>
#include <string>
#include <variant>

namespace obz_market::replay {

namespace {

std::string to_string(side direction) {
    switch (direction) {
    case side::buy:
        return "buy";
    case side::sell:
        return "sell";
    }

    return "unknown";
}

std::ostream& operator<<(std::ostream& out, const symbol& value) {
    return out << value.value;
}

std::ostream& operator<<(std::ostream& out, const order_key& key) {
    return out << key.user.value << ':' << key.client_order.value;
}

std::ostream& operator<<(std::ostream& out, const price& value) {
    return out << value.value;
}

std::ostream& operator<<(std::ostream& out, const quantity& value) {
    return out << value.value;
}

} // namespace

console_writer::console_writer(std::ostream& output)
    : output_(output) {}

void console_writer::write_events(const std::vector<event>& events) {
    for (const auto& emitted : events) {
        write_event(emitted);
    }
}

void console_writer::write_snapshot(const std::optional<book_snapshot>& snapshot) {
    if (!snapshot.has_value()) {
        output_ << "  no book found\n";
        return;
    }

    output_ << "  instrument=" << snapshot->instrument << '\n';
    write_price_levels("bids", snapshot->bids);
    write_price_levels("asks", snapshot->asks);
}

void console_writer::write_event(const event& emitted) {
    std::visit(
        [this](const auto& value) {
            write_event(value);
        },
        emitted
    );
}

void console_writer::write_event(const submit_accepted& value) {
    output_ << "  submit_accepted key=" << value.key << '\n';
}

void console_writer::write_event(const submit_rejected& value) {
    output_ << "  submit_rejected key=" << value.key
            << " reason=\"" << value.reason << "\"\n";
}

void console_writer::write_event(const update_accepted& value) {
    output_ << "  update_accepted key=" << value.key << '\n';
}

void console_writer::write_event(const update_rejected& value) {
    output_ << "  update_rejected key=" << value.key
            << " reason=\"" << value.reason << "\"\n";
}

void console_writer::write_event(const cancel_rejected& value) {
    output_ << "  cancel_rejected key=" << value.key
            << " reason=\"" << value.reason << "\"\n";
}

void console_writer::write_event(const order_cancelled& value) {
    output_ << "  order_cancelled key=" << value.key << '\n';
}

void console_writer::write_event(const trade_executed& value) {
    output_ << "  trade_executed instrument=" << value.instrument
            << " aggressor=" << value.aggressor
            << " resting=" << value.resting
            << " price=" << value.execution_price
            << " size=" << value.executed_size << '\n';
}

void console_writer::write_event(const book_updated& value) {
    output_ << "  book_updated instrument=" << value.instrument
            << " side=" << to_string(value.direction)
            << " best_price=";

    if (value.best_price.has_value()) {
        output_ << *value.best_price;
    } else {
        output_ << "none";
    }

    output_ << " total_size=" << value.total_size << '\n';
}

void console_writer::write_price_levels(
    const char* label,
    const std::vector<price_level_snapshot>& levels
) {
    output_ << "  " << label << ":\n";

    if (levels.empty()) {
        output_ << "    empty\n";
        return;
    }

    for (const auto& level : levels) {
        output_ << "    price=" << level.level_price
                << " total_size=" << level.total_size << '\n';
    }
}

} // namespace obz_market::replay
