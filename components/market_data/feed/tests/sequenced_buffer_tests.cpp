#include <obz/market_lab/market_data/sequenced_buffer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

namespace {

using namespace obz::market_lab::market_data;

constexpr sequenced_buffer_limits test_limits{
    .maximum_pending_values = 4,
    .maximum_sequence_gap = 4
};

} // namespace

TEST_CASE("sequenced buffer leaves an in-order value with its caller", "[market_data][sequence]") {
    sequenced_buffer<std::string> buffer{test_limits, 42};
    std::size_t factory_calls{};

    const auto result = buffer.push(42, [&] {
        ++factory_calls;
        return std::string{"unused"};
    });

    REQUIRE(result == sequence_result::ready);
    REQUIRE(factory_calls == 0);
    REQUIRE(buffer.next_sequence() == 43);
    REQUIRE(buffer.pending_size() == 0);
}

TEST_CASE("sequenced buffer constructs and releases future values in order", "[market_data][sequence]") {
    sequenced_buffer<std::string> buffer{test_limits, 42};
    std::size_t factory_calls{};

    REQUIRE(buffer.push(44, [&] {
        ++factory_calls;
        return std::string{"forty-four"};
    }) == sequence_result::buffered);
    REQUIRE(buffer.push(43, [&] {
        ++factory_calls;
        return std::string{"forty-three"};
    }) == sequence_result::buffered);
    REQUIRE(factory_calls == 2);
    REQUIRE_FALSE(buffer.pop_ready().has_value());

    REQUIRE(buffer.push(42, [] { return std::string{"unused"}; }) == sequence_result::ready);

    auto forty_three = buffer.pop_ready();
    REQUIRE(forty_three == "forty-three");
    auto forty_four = buffer.pop_ready();
    REQUIRE(forty_four == "forty-four");
    REQUIRE_FALSE(buffer.pop_ready().has_value());
    REQUIRE(buffer.next_sequence() == 45);
}

TEST_CASE("sequenced buffer rejects old and duplicate values without constructing them", "[market_data][sequence]") {
    sequenced_buffer<std::string> buffer{test_limits, 42};
    std::size_t factory_calls{};
    auto make_value = [&] {
        ++factory_calls;
        return std::string{"value"};
    };

    REQUIRE(buffer.push(44, make_value) == sequence_result::buffered);
    REQUIRE(buffer.push(44, make_value) == sequence_result::duplicate);
    REQUIRE(buffer.push(41, make_value) == sequence_result::old);
    REQUIRE(factory_calls == 1);
}

TEST_CASE("sequenced buffer enforces gap and capacity limits", "[market_data][sequence]") {
    sequenced_buffer<std::string> buffer{
        sequenced_buffer_limits{
            .maximum_pending_values = 1,
            .maximum_sequence_gap = 2
        },
        42
    };
    std::size_t factory_calls{};
    auto make_value = [&] {
        ++factory_calls;
        return std::string{"value"};
    };

    REQUIRE(buffer.push(45, make_value) == sequence_result::gap_limit_exceeded);
    REQUIRE(buffer.push(43, make_value) == sequence_result::buffered);
    REQUIRE(buffer.push(44, make_value) == sequence_result::capacity_exhausted);
    REQUIRE(factory_calls == 1);
}

TEST_CASE("sequenced buffer can establish its sequence from the first value", "[market_data][sequence]") {
    sequenced_buffer<std::string> buffer{test_limits};

    REQUIRE(buffer.push(100, [] { return std::string{"unused"}; }) == sequence_result::ready);
    REQUIRE(buffer.next_sequence() == 101);
}

TEST_CASE("sequenced buffer supports move-only values", "[market_data][sequence]") {
    sequenced_buffer<std::unique_ptr<int>> buffer{test_limits, 42};

    REQUIRE(buffer.push(43, [] {
        return std::make_unique<int>(7);
    }) == sequence_result::buffered);
    REQUIRE(buffer.push(42, [] {
        return std::make_unique<int>(0);
    }) == sequence_result::ready);

    auto ready = buffer.pop_ready();
    REQUIRE(ready.has_value());
    REQUIRE(**ready == 7);
}

TEST_CASE("sequenced buffer handles the end of the sequence range", "[market_data][sequence]") {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    sequenced_buffer<std::string> buffer{test_limits, maximum};

    REQUIRE(buffer.push(maximum, [] { return std::string{"unused"}; }) == sequence_result::ready);
    REQUIRE_FALSE(buffer.next_sequence().has_value());
    REQUIRE(buffer.push(maximum, [] { return std::string{"unused"}; }) == sequence_result::old);
}
