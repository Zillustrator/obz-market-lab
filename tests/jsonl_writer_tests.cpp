#include <jsonl_writer.hpp>

#include <obz_market/engine.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace obz_market;
using namespace obz_market::replay;

order_key key(std::uint64_t user, std::uint64_t client_order) {
    return order_key{user_id{user}, client_order_id{client_order}};
}

} // namespace

TEST_CASE("jsonl writer writes events", "[replay][jsonl_writer]") {
    std::ostringstream output;
    jsonl_writer writer{output};

    writer.write_events(
        std::vector<event>{
            submit_accepted{key(1, 10)},
            update_accepted{key(1, 10)},
            trade_executed{
                key(2, 20),
                key(1, 10),
                symbol{"ETH-USD"},
                price{100},
                quantity{5}
            },
            book_updated{symbol{"ETH-USD"}, side::sell, price{101}, 3},
            book_updated{symbol{"ETH-USD"}, side::buy, std::nullopt, 0}
        }
    );

    REQUIRE(
        output.str() ==
        R"({"type":"submit_accepted","key":{"user":1,"client_order":10}}
{"type":"update_accepted","key":{"user":1,"client_order":10}}
{"type":"trade_executed","aggressor":{"user":2,"client_order":20},"resting":{"user":1,"client_order":10},"instrument":"ETH-USD","price":100,"size":5}
{"type":"book_updated","instrument":"ETH-USD","side":"sell","best_price":101,"total_size":3}
{"type":"book_updated","instrument":"ETH-USD","side":"buy","best_price":null,"total_size":0}
)"
    );
}

TEST_CASE("jsonl writer escapes string values", "[replay][jsonl_writer]") {
    std::ostringstream output;
    jsonl_writer writer{output};

    writer.write_events(
        std::vector<event>{
            submit_rejected{key(1, 10), "bad \"order\"\nreason\x01"},
            update_rejected{key(1, 10), "bad \"update\"\nreason\x01"}
        }
    );

    REQUIRE(
        output.str() ==
        "{\"type\":\"submit_rejected\",\"key\":{\"user\":1,\"client_order\":10},"
        "\"reason\":\"bad \\\"order\\\"\\nreason\\u0001\"}\n"
        "{\"type\":\"update_rejected\",\"key\":{\"user\":1,\"client_order\":10},"
        "\"reason\":\"bad \\\"update\\\"\\nreason\\u0001\"}\n"
    );
}

TEST_CASE("jsonl writer writes snapshots", "[replay][jsonl_writer]") {
    std::ostringstream output;
    jsonl_writer writer{output};

    writer.write_snapshot(
        book_snapshot{
            symbol{"ETH-USD"},
            std::vector<price_level_snapshot>{
                price_level_snapshot{price{99}, 7}
            },
            std::vector<price_level_snapshot>{
                price_level_snapshot{price{101}, 4}
            }
        }
    );
    writer.write_snapshot(std::nullopt);

    REQUIRE(
        output.str() ==
        R"({"type":"snapshot","snapshot":{"instrument":"ETH-USD","bids":[{"price":99,"total_size":7}],"asks":[{"price":101,"total_size":4}]}}
{"type":"snapshot","snapshot":null}
)"
    );
}
