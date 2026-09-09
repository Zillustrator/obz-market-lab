#include <obz_market/pipeline/engine_runner.hpp>

#include <obz_market/detail/tracing.hpp>

#include <obz/bounded_blocking_queue.hpp>

#include <exception>
#include <stdexcept>
#include <utility>
#include <variant>

namespace obz_market {

namespace {

struct queued_submit {
    submit_order request;
    std::promise<std::vector<event>> response;
};

struct queued_cancel {
    cancel_order request;
    std::promise<std::vector<event>> response;
};

struct queued_update {
    update_order request;
    std::promise<std::vector<event>> response;
};

struct queued_snapshot {
    snapshot_command request;
    std::promise<std::optional<book_snapshot>> response;
};

using queued_command = std::variant<
    std::monostate,
    queued_submit,
    queued_update,
    queued_cancel,
    queued_snapshot
>;

} // namespace

struct engine_runner::implementation {
    explicit implementation(std::size_t command_capacity)
        : commands(command_capacity) {}

    void start() {
        if (closed.load()) {
            throw std::runtime_error("engine_runner cannot be restarted after stop");
        }

        bool expected = false;
        if (!accepting.compare_exchange_strong(expected, true)) {
            throw std::runtime_error("engine_runner is already running");
        }

        worker = std::thread([this] {
            run();
        });
    }

    void stop() {
        if (!accepting.exchange(false)) {
            return;
        }

        closed = true;
        commands.close();

        if (worker.joinable()) {
            worker.join();
        }
    }

    bool running() const noexcept {
        return accepting.load();
    }

    event_future submit(submit_order order) {
        queued_submit queued{
            std::move(order),
            {}
        };
        auto response = queued.response.get_future();
        enqueue(queued_command{std::in_place_type<queued_submit>, std::move(queued)});
        return response;
    }

    event_future update(update_order order) {
        queued_update queued{
            std::move(order),
            {}
        };
        auto response = queued.response.get_future();
        enqueue(queued_command{std::in_place_type<queued_update>, std::move(queued)});
        return response;
    }

    event_future cancel(cancel_order order) {
        queued_cancel queued{
            std::move(order),
            {}
        };
        auto response = queued.response.get_future();
        enqueue(queued_command{std::in_place_type<queued_cancel>, std::move(queued)});
        return response;
    }

    snapshot_future snapshot(snapshot_command command) {
        queued_snapshot queued{
            std::move(command),
            {}
        };
        auto response = queued.response.get_future();
        enqueue(queued_command{std::in_place_type<queued_snapshot>, std::move(queued)});
        return response;
    }

    void enqueue(queued_command command) {
        OBZ_MARKET_TRACE_SCOPE();

        if (!accepting.load()) {
            throw std::runtime_error("engine_runner is not running");
        }

        commands.push(std::move(command));
    }

    void run() {
        OBZ_MARKET_TRACE_SCOPE();

        queued_command command;
        while (commands.wait_and_pop(command)) {
            process(command);
        }
    }

    void process(queued_command& command) {
        OBZ_MARKET_TRACE_SCOPE();

        std::visit(
            [this](auto& value) {
                process(value);
            },
            command
        );
    }

    void process(std::monostate&) {}

    void process(queued_submit& queued) {
        try {
            queued.response.set_value(engine.submit(queued.request));
        } catch (...) {
            queued.response.set_exception(std::current_exception());
        }
    }

    void process(queued_update& queued) {
        try {
            queued.response.set_value(engine.update(queued.request));
        } catch (...) {
            queued.response.set_exception(std::current_exception());
        }
    }

    void process(queued_cancel& queued) {
        try {
            queued.response.set_value(engine.cancel(queued.request));
        } catch (...) {
            queued.response.set_exception(std::current_exception());
        }
    }

    void process(queued_snapshot& queued) {
        try {
            queued.response.set_value(
                engine.snapshot(queued.request.instrument, queued.request.depth)
            );
        } catch (...) {
            queued.response.set_exception(std::current_exception());
        }
    }

    matching_engine engine;
    obz::bounded_blocking_queue<queued_command> commands;
    std::thread worker;
    std::atomic<bool> accepting{false};
    std::atomic<bool> closed{false};
};

engine_runner::engine_runner(std::size_t command_capacity)
    : impl_(std::make_unique<implementation>(command_capacity)) {}

engine_runner::~engine_runner() {
    stop();
}

void engine_runner::start() {
    impl_->start();
}

void engine_runner::stop() {
    impl_->stop();
}

bool engine_runner::running() const noexcept {
    return impl_->running();
}

engine_runner::event_future engine_runner::submit(submit_command command) {
    return impl_->submit(std::move(command.order));
}

engine_runner::event_future engine_runner::update(update_command command) {
    return impl_->update(std::move(command.order));
}

engine_runner::event_future engine_runner::cancel(cancel_command command) {
    return impl_->cancel(std::move(command.order));
}

engine_runner::snapshot_future engine_runner::snapshot(snapshot_command command) {
    return impl_->snapshot(std::move(command));
}

} // namespace obz_market
