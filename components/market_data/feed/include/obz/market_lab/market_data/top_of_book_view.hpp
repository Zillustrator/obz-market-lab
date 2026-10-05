#pragma once

#include <obz/market_lab/market_data/messages.hpp>
#include <obz/market_lab/market_data/top_of_book.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace obz::market_lab::market_data {

enum class top_of_book_state {
    awaiting_snapshot,
    current,
    stale
};

class top_of_book_view {
public:
    void establish(const top_of_book_snapshot& snapshot);
    void apply(std::uint64_t sequence, const book_update& update);
    void invalidate() noexcept;

    top_of_book_state state(symbol_id instrument) const;
    std::optional<top_of_book_snapshot> get(symbol_id instrument) const;

private:
    struct book {
        top_of_book_state state{top_of_book_state::awaiting_snapshot};
        std::uint64_t last_sequence{};
        std::optional<top_of_book_level> best_bid;
        std::optional<top_of_book_level> best_ask;
    };

    struct symbol_id_hash {
        std::size_t operator()(symbol_id instrument) const noexcept;
    };

    std::unordered_map<symbol_id, book, symbol_id_hash> books_;
    bool stream_stale_{};
};

} // namespace obz::market_lab::market_data
