#include <obz/market_lab/market_data/processor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace obz::market_lab::market_data;

processor make_processor(std::uint64_t first_sequence = 1) {
    return processor{
        processor_config{
            .sequence_limits = sequenced_buffer_limits{
                .maximum_pending_values = 4,
                .maximum_sequence_gap = 4
            },
            .first_sequence = first_sequence
        }
    };
}

packet make_packet(std::uint64_t sequence) {
    return packet{
        sequence,
        book_update{std::string{"ETH-USD"}, side::buy, std::uint64_t{100}, 5}
    };
}

} // namespace

TEST_CASE("market-data processor delivers an in-order packet immediately", "[market_data][processor]") {
    auto processor = make_processor();
    std::vector<std::uint64_t> delivered;

    const auto result = processor.process(make_packet(1), [&](packet value) {
        delivered.push_back(value.sequence);
    });

    REQUIRE(result.ordering == sequence_result::ready);
    REQUIRE(result.packets_delivered == 1);
    REQUIRE(delivered == std::vector<std::uint64_t>{1});
    REQUIRE(processor.next_sequence() == 2);
}

TEST_CASE("market-data processor buffers future packets and drains them in order", "[market_data][processor]") {
    auto processor = make_processor();
    std::vector<std::uint64_t> delivered;
    auto consume = [&](packet value) {
        delivered.push_back(value.sequence);
    };

    const auto third = processor.process(make_packet(3), consume);
    const auto second = processor.process(make_packet(2), consume);
    REQUIRE(third.ordering == sequence_result::buffered);
    REQUIRE(second.ordering == sequence_result::buffered);
    REQUIRE(delivered.empty());

    const auto first = processor.process(make_packet(1), consume);
    REQUIRE(first.ordering == sequence_result::ready);
    REQUIRE(first.packets_delivered == 3);
    REQUIRE(delivered == std::vector<std::uint64_t>{1, 2, 3});
    REQUIRE(processor.pending_size() == 0);
    REQUIRE(processor.next_sequence() == 4);
}

TEST_CASE("market-data processor reports discarded packets without delivering them", "[market_data][processor]") {
    auto processor = make_processor();
    std::size_t deliveries{};
    auto consume = [&](packet) {
        ++deliveries;
    };

    REQUIRE(processor.process(make_packet(2), consume).ordering == sequence_result::buffered);
    REQUIRE(processor.process(make_packet(2), consume).ordering == sequence_result::duplicate);
    REQUIRE(processor.process(make_packet(1), consume).packets_delivered == 2);
    REQUIRE(processor.process(make_packet(1), consume).ordering == sequence_result::old);
    REQUIRE(deliveries == 2);
}

TEST_CASE("market-data processor exposes sequence-limit failures", "[market_data][processor]") {
    auto processor = make_processor();

    const auto result = processor.process(make_packet(6), [](packet) {});

    REQUIRE(result.ordering == sequence_result::gap_limit_exceeded);
    REQUIRE(result.packets_delivered == 0);
    REQUIRE(processor.pending_size() == 0);
}
