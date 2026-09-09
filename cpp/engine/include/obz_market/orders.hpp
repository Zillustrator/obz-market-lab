#pragma once

#include <obz_market/types.hpp>

#include <variant>

namespace obz_market {

struct order_key {
    user_id user{};
    client_order_id client_order{};

    bool operator==(const order_key&) const = default;
};

struct limit_order {
    price limit_price;
};

struct market_order {};

using order_pricing = std::variant<limit_order, market_order>;

struct submit_order {
    order_key key;
    symbol instrument;
    side direction{};
    order_pricing pricing;
    quantity size;
};

struct cancel_order {
    order_key key;
};

struct update_order {
    order_key key;
    order_pricing pricing;
    quantity size;
};

} // namespace obz_market
