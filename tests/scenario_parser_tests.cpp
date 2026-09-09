#include <scenario_parser.hpp>

#include <obz_market/pipeline.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using namespace obz_market;
using namespace obz_market::replay;
using Catch::Matchers::ContainsSubstring;

std::filesystem::path write_scenario(
    std::string_view name,
    std::string_view content
) {
    const auto path = std::filesystem::temp_directory_path() / std::string{name};
    std::ofstream output{path};
    REQUIRE(output.is_open());

    output << content;
    output.close();

    return path;
}

template <typename T>
const T& require_command(const scenario_step& step) {
    const auto* command = std::get_if<T>(&step.command);
    REQUIRE(command != nullptr);
    return *command;
}

std::vector<scenario_step> load_scenario(std::string_view content) {
    static int next_id{};
    const auto path = write_scenario(
        "obz-market-scenario-parser-" + std::to_string(++next_id) + ".txt",
        content
    );

    const scenario_parser parser;
    return parser.load(path.string());
}

void require_parse_error(
    std::string_view content,
    const std::string& expected_message
) {
    static int next_id{};
    const auto path = write_scenario(
        "obz-market-scenario-parser-error-" + std::to_string(++next_id) + ".txt",
        content
    );

    const scenario_parser parser;
    REQUIRE_THROWS_WITH(parser.load(path.string()), ContainsSubstring(expected_message));
}

} // namespace

TEST_CASE("scenario parser loads supported commands", "[replay][scenario_parser]") {
    const auto steps = load_scenario(R"(
# comments and blank lines are ignored
submit limit 1 10 ETH-USD sell 100 5
submit market 2 20 ETH-USD buy 3
update limit 1 10 101 4
cancel 1 10
snapshot ETH-USD 5
)");

    REQUIRE(steps.size() == 5);

    const auto& limit_submit = require_command<scenario_submit>(steps[0]);
    REQUIRE(steps[0].line_number == 3);
    REQUIRE(limit_submit.order.key == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(limit_submit.order.instrument == symbol{"ETH-USD"});
    REQUIRE(limit_submit.order.direction == side::sell);
    REQUIRE(limit_submit.order.size == quantity{5});
    REQUIRE(std::holds_alternative<limit_order>(limit_submit.order.pricing));
    REQUIRE(std::get<limit_order>(limit_submit.order.pricing).limit_price == price{100});

    const auto& market_submit = require_command<scenario_submit>(steps[1]);
    REQUIRE(market_submit.order.key == order_key{user_id{2}, client_order_id{20}});
    REQUIRE(market_submit.order.direction == side::buy);
    REQUIRE(market_submit.order.size == quantity{3});
    REQUIRE(std::holds_alternative<market_order>(market_submit.order.pricing));

    const auto& limit_update = require_command<scenario_update>(steps[2]);
    REQUIRE(limit_update.order.key == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(limit_update.order.size == quantity{4});
    REQUIRE(std::holds_alternative<limit_order>(limit_update.order.pricing));
    REQUIRE(std::get<limit_order>(limit_update.order.pricing).limit_price == price{101});

    const auto& cancel = require_command<scenario_cancel>(steps[3]);
    REQUIRE(cancel.order.key == order_key{user_id{1}, client_order_id{10}});

    const auto& snapshot = require_command<scenario_snapshot>(steps[4]);
    REQUIRE(snapshot.command.instrument == symbol{"ETH-USD"});
    REQUIRE(snapshot.command.depth == 5);
}

TEST_CASE("scenario parser supports inline comments", "[replay][scenario_parser]") {
    const auto steps = load_scenario(
        "submit limit 1 10 ETH-USD sell 100 5 # rest ask\n"
    );

    REQUIRE(steps.size() == 1);
    const auto& submit = require_command<scenario_submit>(steps[0]);
    REQUIRE(submit.order.key == order_key{user_id{1}, client_order_id{10}});
}

TEST_CASE("scenario parser accepts flexible whitespace", "[replay][scenario_parser]") {
    const auto steps = load_scenario(
        "  submit   limit   1   10   ETH-USD   sell   100   5  \n"
    );

    REQUIRE(steps.size() == 1);

    const auto& submit = require_command<scenario_submit>(steps[0]);
    REQUIRE(submit.order.key == order_key{user_id{1}, client_order_id{10}});
    REQUIRE(submit.order.instrument == symbol{"ETH-USD"});
    REQUIRE(submit.order.direction == side::sell);
    REQUIRE(submit.order.size == quantity{5});
    REQUIRE(std::get<limit_order>(submit.order.pricing).limit_price == price{100});
}

TEST_CASE("scenario parser rejects malformed commands", "[replay][scenario_parser]") {
    SECTION("unknown command") {
        require_parse_error(
            "replace 1 10 ETH-USD buy 100 1\n",
            "line 1: expected command to be 'submit', 'update', 'cancel', or 'snapshot'"
        );
    }

    SECTION("missing field") {
        require_parse_error(
            "snapshot ETH-USD\n",
            "line 1: missing depth"
        );
    }

    SECTION("invalid side") {
        require_parse_error(
            "submit limit 1 10 ETH-USD bid 100 5\n",
            "line 1: expected side to be 'buy' or 'sell'"
        );
    }

    SECTION("zero price") {
        require_parse_error(
            "submit limit 1 10 ETH-USD buy 0 5\n",
            "line 1: limit_price must be positive"
        );
    }

    SECTION("zero size") {
        require_parse_error(
            "submit market 1 10 ETH-USD buy 0\n",
            "line 1: size must be positive"
        );
    }

    SECTION("extra token") {
        require_parse_error(
            "cancel 1 10 extra\n",
            "line 1: unexpected token 'extra'"
        );
    }

    SECTION("update missing field") {
        require_parse_error(
            "update limit 1 10 100\n",
            "line 1: missing size"
        );
    }

    SECTION("market update") {
        require_parse_error(
            "update market 1 10 5\n",
            "line 1: expected update pricing to be 'limit'"
        );
    }
}
