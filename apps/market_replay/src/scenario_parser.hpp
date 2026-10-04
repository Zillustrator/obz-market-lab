#pragma once

#include <obz/market_lab/matching/runner.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace obz::market_lab::replay {

struct scenario_submit {
    matching::submit_order order;
};

struct scenario_cancel {
    matching::cancel_order order;
};

struct scenario_update {
    matching::update_order order;
};

struct scenario_snapshot {
    matching::snapshot_command command;
};

using scenario_command = std::variant<
    scenario_submit,
    scenario_update,
    scenario_cancel,
    scenario_snapshot
>;

struct scenario_step {
    std::size_t line_number{};
    std::string text;
    scenario_command command;
};

class scenario_parser final {
public:
    std::vector<scenario_step> load(std::string_view path) const;

private:
    static scenario_command parse_command(
        std::string_view line,
        std::size_t line_number
    );
};

} // namespace obz::market_lab::replay
