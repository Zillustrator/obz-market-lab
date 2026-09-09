#pragma once

#include <obz_market/engine.hpp>

#include <iosfwd>
#include <optional>
#include <vector>

namespace obz_market::replay {

class jsonl_writer final {
public:
    explicit jsonl_writer(std::ostream& output);

    void write_events(const std::vector<event>& events);
    void write_snapshot(const std::optional<book_snapshot>& snapshot);

private:
    void write_event(const event& emitted);
    void write_event(const submit_accepted& value);
    void write_event(const submit_rejected& value);
    void write_event(const update_accepted& value);
    void write_event(const update_rejected& value);
    void write_event(const cancel_rejected& value);
    void write_event(const order_cancelled& value);
    void write_event(const trade_executed& value);
    void write_event(const book_updated& value);
    void write_price_levels(const std::vector<price_level_snapshot>& levels);
    void write_key(const order_key& key);

    std::ostream& output_;
};

} // namespace obz_market::replay
