#pragma once

#include <obz_market/orders.hpp>
#include <obz_market/types.hpp>

#include <optional>
#include <string>
#include <variant>

namespace obz_market {

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
    symbol instrument;
    price execution_price;
    quantity executed_size;
};

struct book_updated {
    symbol instrument;
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

} // namespace obz_market
