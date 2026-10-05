#include <obz/market_lab/market_data/top_of_book_view.hpp>

#include <functional>

namespace obz::market_lab::market_data {

void top_of_book_view::establish(const top_of_book_snapshot& snapshot) {
    books_.insert_or_assign(snapshot.instrument, book{
        top_of_book_state::current,
        snapshot.reference_sequence,
        snapshot.best_bid,
        snapshot.best_ask
    });
}

void top_of_book_view::invalidate() noexcept {
    stream_stale_ = true;
    for (auto& [instrument, value] : books_) {
        static_cast<void>(instrument);
        value.state = top_of_book_state::stale;
    }
}

top_of_book_state top_of_book_view::state(symbol_id instrument) const {
    const auto found = books_.find(instrument);
    if (found != books_.end()) {
        return found->second.state;
    }
    return stream_stale_
        ? top_of_book_state::stale
        : top_of_book_state::awaiting_snapshot;
}

std::optional<top_of_book_snapshot> top_of_book_view::get(symbol_id instrument) const {
    const auto found = books_.find(instrument);
    if (found == books_.end() || found->second.state != top_of_book_state::current) {
        return std::nullopt;
    }
    return top_of_book_snapshot{
        instrument,
        found->second.last_sequence,
        found->second.best_bid,
        found->second.best_ask
    };
}

void top_of_book_view::apply(std::uint64_t sequence, const book_update& update) {
    const auto found = books_.find(update.instrument);
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

std::size_t top_of_book_view::symbol_id_hash::operator()(symbol_id instrument) const noexcept {
    return std::hash<std::uint32_t>{}(instrument.value);
}

} // namespace obz::market_lab::market_data
