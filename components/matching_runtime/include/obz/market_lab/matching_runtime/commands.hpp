#pragma once

#include <obz/market_lab/matching.hpp>

#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace obz::market_lab::matching_runtime {

struct submit_command {
    matching::submit_order order;
};

struct cancel_command {
    matching::cancel_order order;
};

struct update_command {
    matching::update_order order;
};

struct snapshot_command {
    matching::symbol instrument;
    std::size_t depth{};
};

using engine_command = std::variant<
    submit_command,
    update_command,
    cancel_command,
    snapshot_command
>;

struct snapshot_response {
    std::optional<matching::book_snapshot> snapshot;
};

using engine_response = std::variant<
    std::vector<matching::event>,
    snapshot_response
>;

} // namespace obz::market_lab::matching_runtime
