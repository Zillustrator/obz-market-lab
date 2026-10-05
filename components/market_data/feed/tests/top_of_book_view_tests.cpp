#include <obz/market_lab/market_data/top_of_book_view.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace {

using namespace obz::market_lab::market_data;

top_of_book_snapshot make_snapshot(std::uint64_t reference_sequence) {
    return top_of_book_snapshot{
        symbol_id{1},
        reference_sequence,
        top_of_book_level{100, 5},
        top_of_book_level{101, 7}
    };
}

book_update make_update(
    side direction,
    std::optional<std::uint64_t> price,
    std::uint64_t aggregate_size
) {
    return book_update{symbol_id{1}, direction, price, aggregate_size};
}

} // namespace

TEST_CASE("top-of-book view requires a snapshot before exposing state", "[market_data][view]") {
    top_of_book_view view;

    view.apply(1, make_update(side::buy, 100, 5));

    REQUIRE(view.state(symbol_id{1}) == top_of_book_state::awaiting_snapshot);
    REQUIRE_FALSE(view.get(symbol_id{1}).has_value());
}

TEST_CASE("top-of-book view applies updates after its snapshot", "[market_data][view]") {
    top_of_book_view view;
    view.establish(make_snapshot(10));
    view.apply(11, make_update(side::buy, 99, 8));
    view.apply(12, make_update(side::sell, std::nullopt, 0));

    const auto current = view.get(symbol_id{1});
    REQUIRE(view.state(symbol_id{1}) == top_of_book_state::current);
    REQUIRE(current.has_value());
    REQUIRE(current->reference_sequence == 12);
    REQUIRE(current->best_bid == top_of_book_level{99, 8});
    REQUIRE_FALSE(current->best_ask.has_value());
}

TEST_CASE("top-of-book view hides stale state until it receives a new snapshot", "[market_data][view]") {
    top_of_book_view view;
    view.establish(make_snapshot(10));
    view.invalidate();
    view.apply(12, make_update(side::buy, 99, 8));

    REQUIRE(view.state(symbol_id{1}) == top_of_book_state::stale);
    REQUIRE(view.state(symbol_id{2}) == top_of_book_state::stale);
    REQUIRE_FALSE(view.get(symbol_id{1}).has_value());

    view.establish(top_of_book_snapshot{
        symbol_id{1}, 19, std::nullopt, top_of_book_level{101, 4}
    });
    REQUIRE(view.state(symbol_id{1}) == top_of_book_state::current);
    REQUIRE(view.get(symbol_id{1})->best_ask == top_of_book_level{101, 4});
}
