#pragma once

#include <obz/market_lab/matching/orders.hpp>
#include <obz/market_lab/matching/types.hpp>

#include <optional>
#include <string>
#include <variant>

namespace obz::market_lab::matching {

struct submit_accepted {
    order_key key;
};

struct submit_rejected {
    order_key key;
    std::string reason;
};

struct update_accepted {
    order_key key;
};

struct update_rejected {
    order_key key;
    std::string reason;
};

struct cancel_rejected {
    order_key key;
    std::string reason;
};

struct order_cancelled {
    order_key key;
};

struct trade_executed {
    order_key aggressor;
    order_key resting;
    symbol_id instrument;
    price execution_price;
    quantity executed_size;
};

struct book_updated {
    symbol_id instrument;
    side direction{};
    std::optional<price> best_price;
    std::uint64_t total_size{};
};

using event = std::variant<
    submit_accepted,
    submit_rejected,
    update_accepted,
    update_rejected,
    cancel_rejected,
    order_cancelled,
    trade_executed,
    book_updated
>;

} // namespace obz::market_lab::matching
