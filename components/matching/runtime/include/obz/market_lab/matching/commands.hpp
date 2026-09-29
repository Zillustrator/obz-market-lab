#pragma once

#include <obz/market_lab/matching/engine.hpp>

#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace obz::market_lab::matching {

struct submit_command {
    submit_order order;
};

struct cancel_command {
    cancel_order order;
};

struct update_command {
    update_order order;
};

struct snapshot_command {
    symbol instrument;
    std::size_t depth{};
};

using engine_command = std::variant<
    submit_command,
    update_command,
    cancel_command,
    snapshot_command
>;

struct snapshot_response {
    std::optional<book_snapshot> snapshot;
};

using engine_response = std::variant<
    std::vector<event>,
    snapshot_response
>;

} // namespace obz::market_lab::matching
