#pragma once

#include <obz/market_lab/market_data/messages.hpp>
#include <obz/market_lab/market_data/top_of_book.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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

    top_of_book_state state(std::string_view symbol) const;
    std::optional<top_of_book_snapshot> get(std::string_view symbol) const;

private:
    struct book {
        top_of_book_state state{top_of_book_state::awaiting_snapshot};
        std::uint64_t last_sequence{};
        std::optional<top_of_book_level> best_bid;
        std::optional<top_of_book_level> best_ask;
    };

    std::unordered_map<std::string, book> books_;
    bool stream_stale_{};
};

} // namespace obz::market_lab::market_data
