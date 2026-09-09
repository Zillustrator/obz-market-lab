#pragma once

#include <obz_market/events.hpp>
#include <obz_market/orders.hpp>
#include <obz_market/types.hpp>

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <vector>

namespace obz_market {

class order_book {
public:
    struct resting_order {
        order_key key;
        side direction{};
        price limit_price;
        quantity open_size;
        sequence_number sequence{};
    };

    using resting_order_iterator = std::list<resting_order>::iterator;

    class resting_order_handle {
    public:
        resting_order_handle() = default;

        bool has_value() const noexcept {
            return direction_.has_value();
        }

        explicit operator bool() const noexcept {
            return has_value();
        }

    private:
        friend class order_book;

        resting_order_handle(
            side direction,
            price limit_price,
            resting_order_iterator order
        )
            : direction_(direction),
              limit_price_(limit_price),
              order_(order) {}

        std::optional<side> direction_;
        std::optional<price> limit_price_;
        resting_order_iterator order_;
    };

    struct submit_result {
        std::vector<event> events;
        resting_order_handle resting_order;
        std::vector<order_key> filled_resting_orders;
    };

    struct cancel_result {
        std::vector<event> events;
        bool cancelled{};
    };

    struct update_result {
        std::vector<event> events;
        resting_order_handle resting_order;
        std::vector<order_key> filled_resting_orders;
        bool updated{};
    };

    struct price_level_snapshot {
        price level_price;
        std::uint64_t total_size{};

        bool operator==(const price_level_snapshot&) const = default;
    };

    struct order_book_snapshot {
        symbol instrument;
        std::vector<price_level_snapshot> bids;
        std::vector<price_level_snapshot> asks;

        bool operator==(const order_book_snapshot&) const = default;
    };

    explicit order_book(symbol instrument);

    submit_result submit(const submit_order& order, sequence_number sequence);
    update_result update(
        const update_order& order,
        const resting_order_handle& handle,
        sequence_number sequence
    );
    cancel_result cancel(const order_key& key, const resting_order_handle& handle);
    order_book_snapshot snapshot(std::size_t depth) const;

private:
    struct incoming_order {
        order_key key;
        side direction{};
        quantity open_size;
        sequence_number sequence{};
    };

    struct price_level {
        std::uint64_t total_size{};
        std::list<resting_order> orders;
    };

    struct top_of_book_state {
        price best_price;
        std::uint64_t total_size{};

        bool operator==(const top_of_book_state&) const = default;
    };

    using bid_levels = std::map<price, price_level, std::greater<>>;
    using ask_levels = std::map<price, price_level, std::less<>>;

    submit_result submit_buy(const submit_order& order, sequence_number sequence);
    submit_result submit_sell(const submit_order& order, sequence_number sequence);

    update_result update_buy(
        const update_order& order,
        const resting_order_handle& handle,
        sequence_number sequence
    );
    update_result update_sell(
        const update_order& order,
        const resting_order_handle& handle,
        sequence_number sequence
    );

    cancel_result cancel_bid(const order_key& key, const resting_order_handle& handle);
    cancel_result cancel_ask(const order_key& key, const resting_order_handle& handle);

    static std::optional<price> limit_price(const order_pricing& pricing);
    static bool can_buy_cross(const order_pricing& pricing, price best_ask);
    static bool can_sell_cross(const order_pricing& pricing, price best_bid);

    template <typename Levels>
    bool cancel_from_levels(
        const order_key& key,
        const resting_order_handle& handle,
        Levels& levels,
        std::vector<event>& events
    );

    template <typename Levels>
    bool reduce_order_size(
        const update_order& order,
        const resting_order_handle& handle,
        Levels& levels
    );

    template <typename Levels>
    bool remove_order(
        const order_key& key,
        const resting_order_handle& handle,
        Levels& levels
    );

    template <typename CanCross, typename OwnLevels, typename OppositeLevels>
    update_result replace_order(
        const update_order& order,
        const resting_order_handle& handle,
        side direction,
        CanCross can_cross,
        OwnLevels& own_levels,
        OppositeLevels& opposite_levels,
        sequence_number sequence
    );

    template <typename CanCross, typename Levels>
    void match_order(
        const order_pricing& pricing,
        CanCross can_cross,
        incoming_order& incoming,
        Levels& levels,
        std::vector<event>& events,
        std::vector<order_key>& filled_resting_orders
    );

    void match_against_level(
        incoming_order& incoming,
        price_level& level,
        std::vector<event>& events,
        std::vector<order_key>& filled_resting_orders
    );

    resting_order_handle rest_order(const incoming_order& incoming, price limit_price);

    template <typename Levels>
    std::optional<top_of_book_state> top_of_book(const Levels& levels) const;

    template <typename Levels>
    static std::vector<price_level_snapshot> snapshot_levels(
        const Levels& levels,
        std::size_t depth
    );

    void append_book_update_if_changed(
        side direction,
        const std::optional<top_of_book_state>& before,
        const std::optional<top_of_book_state>& after,
        std::vector<event>& events
    ) const;

    symbol instrument_;
    bid_levels bids_;
    ask_levels asks_;
};

} // namespace obz_market
