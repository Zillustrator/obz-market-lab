#pragma once

#include <obz/market_lab/market_data/codec.hpp>
#include <obz/market_lab/market_data/sequenced_buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace obz::market_lab::market_data {

struct processor_config {
    sequenced_buffer_limits sequence_limits;
    std::optional<std::uint64_t> first_sequence;
};

struct processing_result {
    sequence_result ordering{};
    std::size_t packets_delivered{};
};

class processor {
public:
    explicit processor(processor_config config)
        : packets_{config.sequence_limits, config.first_sequence} {}

    template <typename Consumer>
    processing_result process(packet value, Consumer&& consume) {
        const auto ordering = packets_.push(value.sequence, [&] {
            return std::move(value);
        });

        processing_result result{ordering};
        if (ordering != sequence_result::ready) {
            return result;
        }

        std::invoke(consume, std::move(value));
        ++result.packets_delivered;

        while (auto queued = packets_.pop_ready()) {
            std::invoke(consume, std::move(*queued));
            ++result.packets_delivered;
        }

        return result;
    }

    std::optional<std::uint64_t> next_sequence() const noexcept {
        return packets_.next_sequence();
    }

    std::size_t pending_size() const noexcept {
        return packets_.pending_size();
    }

private:
    sequenced_buffer<packet> packets_;
};

} // namespace obz::market_lab::market_data
