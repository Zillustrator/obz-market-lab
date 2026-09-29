#pragma once

#include <obz/market_lab/matching.hpp>
#include <obz/market_lab/matching_runtime/commands.hpp>

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace obz::market_lab::matching_runtime {

class engine_runner {
public:
    using event_future = std::future<std::vector<matching::event>>;
    using snapshot_future = std::future<std::optional<matching::book_snapshot>>;

    explicit engine_runner(std::size_t command_capacity);
    ~engine_runner();

    engine_runner(const engine_runner&) = delete;
    engine_runner& operator=(const engine_runner&) = delete;

    engine_runner(engine_runner&&) = delete;
    engine_runner& operator=(engine_runner&&) = delete;

    void start();
    void stop();

    bool running() const noexcept;

    event_future submit(submit_command command);

    event_future update(update_command command);

    event_future cancel(cancel_command command);

    snapshot_future snapshot(snapshot_command command);

private:
    struct implementation;

    std::unique_ptr<implementation> impl_;
};

} // namespace obz::market_lab::matching_runtime
