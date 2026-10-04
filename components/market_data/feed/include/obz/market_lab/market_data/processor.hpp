#pragma once

#include <obz/market_lab/market_data/codec.hpp>
#include <obz/market_lab/market_data/sequenced_buffer.hpp>
#include <obz/market_lab/tracing/trace.hpp>

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
        OBZ_MARKET_LAB_TRACE_SCOPE_N("market_data.process");

        const auto ordering = [&] {
            OBZ_MARKET_LAB_TRACE_SCOPE_N("market_data.sequence");
            return packets_.push(value.sequence, [&] {
                return std::move(value);
            });
        }();

        processing_result result{ordering};
        if (ordering != sequence_result::ready) {
            return result;
        }

        invoke_consumer(consume, std::move(value));
        ++result.packets_delivered;

        while (auto queued = [&] {
            OBZ_MARKET_LAB_TRACE_SCOPE_N("market_data.sequence");
            return packets_.pop_ready();
        }()) {
            invoke_consumer(consume, std::move(*queued));
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
    template <typename Consumer>
    static void invoke_consumer(Consumer& consume, packet&& value) {
        OBZ_MARKET_LAB_TRACE_SCOPE_N("market_data.consume");
        std::invoke(consume, std::move(value));
    }

    sequenced_buffer<packet> packets_;
};

} // namespace obz::market_lab::market_data
