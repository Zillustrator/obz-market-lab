#pragma once

#include <obz/market_lab/matching.hpp>

#include <iosfwd>
#include <optional>
#include <vector>

namespace obz::market_lab::replay {

class console_writer final {
public:
    explicit console_writer(std::ostream& output);

    void write_events(const std::vector<matching::event>& events);
    void write_snapshot(const std::optional<matching::book_snapshot>& snapshot);

private:
    void write_event(const matching::event& emitted);
    void write_event(const matching::submit_accepted& value);
    void write_event(const matching::submit_rejected& value);
    void write_event(const matching::update_accepted& value);
    void write_event(const matching::update_rejected& value);
    void write_event(const matching::cancel_rejected& value);
    void write_event(const matching::order_cancelled& value);
    void write_event(const matching::trade_executed& value);
    void write_event(const matching::book_updated& value);
    void write_price_levels(
        const char* label,
        const std::vector<matching::price_level_snapshot>& levels
    );

    std::ostream& output_;
};

} // namespace obz::market_lab::replay
