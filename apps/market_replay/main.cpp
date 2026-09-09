#include "console_writer.hpp"
#include "jsonl_writer.hpp"
#include "scenario_parser.hpp"

#include <obz_market/pipeline.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace obz_market;
using namespace obz_market::replay;

enum class output_format {
    text,
    jsonl
};

struct command_line_options {
    output_format format{output_format::text};
    std::filesystem::path scenario_file;
    std::filesystem::path scenario_directory;
};

submit_order make_limit_order(
    std::uint64_t user,
    std::uint64_t client_order,
    std::string instrument,
    side direction,
    std::uint64_t limit_price,
    std::uint64_t size
) {
    return submit_order{
        order_key{user_id{user}, client_order_id{client_order}},
        symbol{std::move(instrument)},
        direction,
        order_pricing{std::in_place_type<limit_order>, price{limit_price}},
        quantity{size}
    };
}

cancel_order make_cancel_order(std::uint64_t user, std::uint64_t client_order) {
    return cancel_order{
        order_key{user_id{user}, client_order_id{client_order}}
    };
}

command_line_options parse_command_line(int argc, char** argv) {
    command_line_options options;

    for (int index = 1; index < argc; ++index) {
        const auto argument = std::string_view{argv[index]};

        if (argument == "--format") {
            if (index + 1 >= argc) {
                throw std::runtime_error{"missing value for --format"};
            }

            const auto value = std::string_view{argv[++index]};
            if (value == "text") {
                options.format = output_format::text;
            } else if (value == "jsonl") {
                options.format = output_format::jsonl;
            } else {
                throw std::runtime_error{"expected --format to be 'text' or 'jsonl'"};
            }

            continue;
        }

        if (argument == "--file") {
            if (index + 1 >= argc) {
                throw std::runtime_error{"missing value for --file"};
            }

            if (!options.scenario_file.empty() || !options.scenario_directory.empty()) {
                throw std::runtime_error{"only one scenario source can be provided"};
            }

            options.scenario_file = argv[++index];
            continue;
        }

        if (argument == "--directory") {
            if (index + 1 >= argc) {
                throw std::runtime_error{"missing value for --directory"};
            }

            if (!options.scenario_file.empty() || !options.scenario_directory.empty()) {
                throw std::runtime_error{"only one scenario source can be provided"};
            }

            options.scenario_directory = argv[++index];
            continue;
        }

        throw std::runtime_error{"unexpected positional argument"};
    }

    return options;
}

template <typename Writer>
void execute_command(
    engine_runner& runner,
    Writer& writer,
    const scenario_submit& command
) {
    writer.write_events(runner.submit(submit_command{command.order}).get());
}

template <typename Writer>
void execute_command(
    engine_runner& runner,
    Writer& writer,
    const scenario_cancel& command
) {
    writer.write_events(runner.cancel(cancel_command{command.order}).get());
}

template <typename Writer>
void execute_command(
    engine_runner& runner,
    Writer& writer,
    const scenario_update& command
) {
    writer.write_events(runner.update(update_command{command.order}).get());
}

template <typename Writer>
void execute_command(
    engine_runner& runner,
    Writer& writer,
    const scenario_snapshot& command
) {
    writer.write_snapshot(runner.snapshot(command.command).get());
}

template <typename Writer>
void execute_step(
    engine_runner& runner,
    Writer& writer,
    const scenario_step& step,
    bool print_step_labels
) {
    if (print_step_labels) {
        if (step.line_number == 0) {
            std::cout << "\n" << step.text << '\n';
        } else {
            std::cout << "\nline " << step.line_number << ": " << step.text << '\n';
        }
    }

    std::visit(
        [&runner, &writer](const auto& command) {
            execute_command(runner, writer, command);
        },
        step.command
    );
}

template <typename Writer>
void run_steps(
    engine_runner& runner,
    Writer& writer,
    const std::vector<scenario_step>& steps,
    bool print_step_labels
) {
    for (const auto& step : steps) {
        execute_step(runner, writer, step, print_step_labels);
    }
}

std::vector<scenario_step> default_scenario() {
    return {
        scenario_step{
            0,
            "1. Rest a sell limit order at 100",
            scenario_submit{
                make_limit_order(1, 10, "ETH-USD", side::sell, 100, 5)
            }
        },
        scenario_step{
            0,
            "2. Rest a buy limit order at 99",
            scenario_submit{
                make_limit_order(2, 20, "ETH-USD", side::buy, 99, 3)
            }
        },
        scenario_step{
            0,
            "3. Snapshot depth 5",
            scenario_snapshot{snapshot_command{symbol{"ETH-USD"}, 5}}
        },
        scenario_step{
            0,
            "4. Submit a buy limit order that crosses the ask",
            scenario_submit{
                make_limit_order(3, 30, "ETH-USD", side::buy, 101, 4)
            }
        },
        scenario_step{
            0,
            "5. Snapshot depth 5",
            scenario_snapshot{snapshot_command{symbol{"ETH-USD"}, 5}}
        },
        scenario_step{
            0,
            "6. Reduce the resting bid size",
            scenario_update{
                update_order{
                    order_key{user_id{2}, client_order_id{20}},
                    order_pricing{std::in_place_type<limit_order>, price{99}},
                    quantity{2}
                }
            }
        },
        scenario_step{
            0,
            "7. Cancel the resting bid",
            scenario_cancel{make_cancel_order(2, 20)}
        },
        scenario_step{
            0,
            "8. Final snapshot depth 5",
            scenario_snapshot{snapshot_command{symbol{"ETH-USD"}, 5}}
        }
    };
}

std::vector<std::filesystem::path> scenario_files_in(const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory)) {
        throw std::runtime_error{"scenario path does not exist"};
    }

    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error{"scenario path is not a directory"};
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator{directory}) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

template <typename Writer>
void run_default_demo(Writer& writer, bool print_metadata) {
    engine_runner runner{128};
    runner.start();

    if (print_metadata) {
        std::cout << "Scenario: build an ETH-USD book, cross the ask side, then cancel the bid.\n";
    }

    run_steps(runner, writer, default_scenario(), print_metadata);

    runner.stop();
}

template <typename Writer>
void run_scenario_file(
    Writer& writer,
    const std::filesystem::path& path,
    bool print_metadata
) {
    const scenario_parser parser;
    const auto steps = parser.load(path.string());
    engine_runner runner{128};
    runner.start();

    if (print_metadata) {
        std::cout << "Scenario file: " << path.string() << '\n';
        std::cout << "Commands: " << steps.size() << '\n';
    }

    run_steps(runner, writer, steps, print_metadata);

    runner.stop();
}

template <typename Writer>
void run_scenario_directory(
    Writer& writer,
    const std::filesystem::path& directory,
    bool print_metadata
) {
    const auto files = scenario_files_in(directory);

    if (print_metadata) {
        std::cout << "Scenario directory: " << directory.string() << '\n';
        std::cout << "Files: " << files.size() << '\n';
    }

    for (const auto& file : files) {
        run_scenario_file(writer, file, print_metadata);
    }
}

template <typename Writer>
void run_replay(
    Writer& writer,
    const command_line_options& options,
    bool print_metadata
) {
    if (options.scenario_file.empty() && options.scenario_directory.empty()) {
        run_default_demo(writer, print_metadata);
    } else if (!options.scenario_file.empty()) {
        run_scenario_file(writer, options.scenario_file, print_metadata);
    } else {
        run_scenario_directory(writer, options.scenario_directory, print_metadata);
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_command_line(argc, argv);

        if (options.format == output_format::text) {
            console_writer writer{std::cout};
            std::cout << "Obz Market Lab replay demo\n";
            run_replay(writer, options, true);
        } else {
            jsonl_writer writer{std::cout};
            run_replay(writer, options, false);
        }
    } catch (const std::exception& error) {
        std::cerr << "Replay failed: " << error.what() << '\n';
        std::cerr << "Usage: " << argv[0]
                  << " [--file path | --directory path] [--format text|jsonl]\n";
        return 1;
    }

    return 0;
}
