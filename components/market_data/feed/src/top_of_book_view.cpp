#include <obz/market_lab/market_data/top_of_book_view.hpp>

namespace obz::market_lab::market_data {

void top_of_book_view::establish(const top_of_book_snapshot& snapshot) {
    books_.insert_or_assign(snapshot.symbol, book{
        top_of_book_state::current,
        snapshot.reference_sequence,
        snapshot.best_bid,
        snapshot.best_ask
    });
}

void top_of_book_view::invalidate() noexcept {
    stream_stale_ = true;
    for (auto& [symbol, value] : books_) {
        static_cast<void>(symbol);
        value.state = top_of_book_state::stale;
    }
}

top_of_book_state top_of_book_view::state(std::string_view symbol) const {
    const auto found = books_.find(std::string{symbol});
    if (found != books_.end()) {
        return found->second.state;
    }
    return stream_stale_
        ? top_of_book_state::stale
        : top_of_book_state::awaiting_snapshot;
}

std::optional<top_of_book_snapshot> top_of_book_view::get(std::string_view symbol) const {
    const auto found = books_.find(std::string{symbol});
    if (found == books_.end() || found->second.state != top_of_book_state::current) {
        return std::nullopt;
    }
    return top_of_book_snapshot{
        found->first,
        found->second.last_sequence,
        found->second.best_bid,
        found->second.best_ask
    };
}

void top_of_book_view::apply(std::uint64_t sequence, const book_update& update) {
    const auto found = books_.find(update.symbol);
    if (found == books_.end() || found->second.state != top_of_book_state::current ||
        sequence <= found->second.last_sequence) {
        return;
    }

    auto& book = found->second;
    std::optional<top_of_book_level> level;
    if (update.best_price.has_value()) {
        level.emplace(*update.best_price, update.aggregate_size);
    }
    if (update.direction == side::buy) {
        book.best_bid = level;
    } else {
        book.best_ask = level;
    }
    book.last_sequence = sequence;
}

} // namespace obz::market_lab::market_data
