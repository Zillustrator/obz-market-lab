#pragma once

#include <obz_market/pipeline.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace obz_market::replay {

struct scenario_submit {
    submit_order order;
};

struct scenario_cancel {
    cancel_order order;
};

struct scenario_update {
    update_order order;
};

struct scenario_snapshot {
    snapshot_command command;
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

} // namespace obz_market::replay
