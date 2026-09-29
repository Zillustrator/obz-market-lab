#pragma once

#include <obz/market_lab/matching/commands.hpp>

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace obz::market_lab::matching {

class runner {
public:
    using event_future = std::future<std::vector<event>>;
    using snapshot_future = std::future<std::optional<book_snapshot>>;

    explicit runner(std::size_t command_capacity);
    ~runner();

    runner(const runner&) = delete;
    runner& operator=(const runner&) = delete;

    runner(runner&&) = delete;
    runner& operator=(runner&&) = delete;

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

} // namespace obz::market_lab::matching
