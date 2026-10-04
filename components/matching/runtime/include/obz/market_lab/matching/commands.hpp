#pragma once

#include <obz/market_lab/matching/engine.hpp>

#include <cstddef>

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

} // namespace obz::market_lab::matching
