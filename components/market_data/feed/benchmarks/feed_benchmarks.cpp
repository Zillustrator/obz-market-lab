#include <obz/market_lab/market_data/codec.hpp>
#include <obz/market_lab/market_data/processor.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace obz::market_lab::market_data;

constexpr std::size_t sequence_capacity{64};
constexpr std::uint64_t maximum_sequence_gap{64};

packet make_packet(std::uint64_t sequence) {
    return packet{
        sequence,
        book_update{
            .instrument = symbol_id{1},
            .direction = side::buy,
            .best_price = 2'500,
            .aggregate_size = 10
        }
    };
}

processor make_processor(std::uint64_t first_sequence = 1) {
    return processor{
        processor_config{
            .sequence_limits = sequenced_buffer_limits{
                .maximum_pending_values = sequence_capacity,
                .maximum_sequence_gap = maximum_sequence_gap
            },
            .first_sequence = first_sequence
        }
    };
}

packet decode_packet(const std::vector<std::byte>& bytes) {
    auto result = decode(bytes);
    if (const auto* error = std::get_if<decode_error>(&result)) {
        throw std::runtime_error{std::string{to_string(*error)}};
    }
    return std::move(std::get<packet>(result));
}

std::vector<std::vector<std::byte>> make_encoded_packets(std::size_t count) {
    std::vector<std::vector<std::byte>> packets;
    packets.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        packets.push_back(encode(make_packet(index + 1)));
    }
    return packets;
}

void decode_book_update(benchmark::State& state) {
    const auto bytes = encode(make_packet(1));

    for (auto _ : state) {
        benchmark::DoNotOptimize(decode(bytes));
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(
        state.iterations() * static_cast<std::int64_t>(bytes.size()));
}

void synchronous_in_order(benchmark::State& state) {
    constexpr std::size_t packet_count{4'096};
    const auto encoded_packets = make_encoded_packets(packet_count);
    auto packet_processor = make_processor();
    std::size_t packet_index{};
    std::size_t consumed{};
    std::uint64_t sequence_sum{};

    for (auto _ : state) {
        if (packet_index == encoded_packets.size()) {
            state.PauseTiming();
            packet_processor = make_processor();
            packet_index = 0;
            state.ResumeTiming();
        }

        auto value = decode_packet(encoded_packets[packet_index++]);
        packet_processor.process(
            std::move(value),
            [&](const packet& delivered) {
                sequence_sum += delivered.sequence;
                ++consumed;
            });
    }

    benchmark::DoNotOptimize(consumed);
    benchmark::DoNotOptimize(sequence_sum);
    state.SetItemsProcessed(state.iterations());
}

void synchronous_burst(benchmark::State& state) {
    const auto burst_size = static_cast<std::size_t>(state.range(0));
    const auto encoded_packets = make_encoded_packets(burst_size);
    std::size_t consumed{};
    std::uint64_t sequence_sum{};

    for (auto _ : state) {
        state.PauseTiming();
        auto packet_processor = make_processor();
        state.ResumeTiming();

        for (const auto& bytes : encoded_packets) {
            auto value = decode_packet(bytes);
            packet_processor.process(
                std::move(value),
                [&](const packet& delivered) {
                    sequence_sum += delivered.sequence;
                    ++consumed;
                });
        }
    }

    benchmark::DoNotOptimize(consumed);
    benchmark::DoNotOptimize(sequence_sum);
    state.SetItemsProcessed(
        state.iterations() * static_cast<std::int64_t>(burst_size));
}

BENCHMARK(decode_book_update);
BENCHMARK(synchronous_in_order);
BENCHMARK(synchronous_burst)->Arg(16)->Arg(64)->Arg(256);

} // namespace
