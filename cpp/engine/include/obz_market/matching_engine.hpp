#pragma once

#include <obz_market/events.hpp>
#include <obz_market/order_book.hpp>
#include <obz_market/orders.hpp>
#include <obz_market/types.hpp>

#include <unordered_map>
#include <vector>

namespace obz_market {

using book_snapshot = order_book::order_book_snapshot;
using price_level_snapshot = order_book::price_level_snapshot;

class matching_engine {
public:
    matching_engine() = default;

    std::vector<event> submit(const submit_order& order);
    std::vector<event> update(const update_order& order);
    std::vector<event> cancel(const cancel_order& order);
    std::optional<book_snapshot> snapshot(
        const symbol& instrument,
        std::size_t depth
    ) const;

private:
    struct order_key_hash {
        std::size_t operator()(const order_key& key) const noexcept;
    };

    struct symbol_hash {
        std::size_t operator()(const symbol& instrument) const noexcept;
    };

    struct active_order {
        symbol instrument;
        order_book::resting_order_handle handle;
    };

    using book_map = std::unordered_map<symbol, order_book, symbol_hash>;
    using active_order_map = std::unordered_map<order_key, active_order, order_key_hash>;

    static std::optional<std::string> validate(const submit_order& order);
    static std::optional<std::string> validate(const update_order& order);
    static std::vector<event> reject_submit(const order_key& key, std::string reason);
    static std::vector<event> reject_update(const order_key& key, std::string reason);
    static std::vector<event> reject_cancel(const order_key& key, std::string reason);

    order_book& get_or_create_book(const symbol& instrument);
    order_book* find_book(const symbol& instrument);
    const order_book* find_book(const symbol& instrument) const;

    void record_submit_result(
        const submit_order& order,
        const order_book::submit_result& result
    );
    void record_update_result(
        active_order_map::iterator active,
        const order_book::update_result& result
    );
    void record_cancel_result(
        active_order_map::iterator active,
        const order_book::cancel_result& result
    );

    book_map books_;
    active_order_map active_orders_;
    sequence_number next_sequence_{sequence_number{1}};
};

} // namespace obz_market
