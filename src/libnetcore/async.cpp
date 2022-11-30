#include <netcore/async.hpp>
#include <netcore/except.hpp>
#include <netcore/runtime.hpp>
#include <netcore/signalfd.h>

#include <array>
#include <csignal>
#include <timber/timber>

using namespace std::chrono_literals;

namespace {
    constexpr auto stop_signals = std::array {
        SIGINT,
        SIGTERM
    };

    constexpr auto default_options = netcore::async_options {
        .max_events = SOMAXCONN,
        .timeout = 90s,
        .signals = stop_signals
    };

    inline auto runtime() -> netcore::runtime& {
        return netcore::runtime::current();
    }

    auto async_entry(
        ext::task<>&& task,
        std::exception_ptr& exception
    ) -> ext::detached_task {
        auto t = std::forward<ext::task<>>(task);

        try {
            co_await t;
        }
        catch (const netcore::task_canceled&) {
            TIMBER_DEBUG("main task canceled");
        }
        catch (...) {
            exception = std::current_exception();
        }

        runtime().stop();
    }

    auto wait_for_signal(std::span<const int> signals) -> ext::detached_task {
        auto sig = netcore::signalfd::create(signals);
        auto signal = 0;

        while (true) {
            signal = co_await sig.wait_for_signal();
            if (signal <= 0) break;

            TIMBER_INFO("signal ({}): {}", signal, strsignal(signal));

            if (runtime().shutting_down()) runtime().stop();
            else runtime().shutdown();

            if (runtime().one_or_empty()) break;
        }
    }
}

namespace netcore {
    auto async(ext::task<>&& task) -> void {
        async(default_options, std::forward<ext::task<>>(task));
    }

    auto async(
        const async_options& options,
        ext::task<>&& task
    ) -> void {
        auto rt = runtime(runtime_options {
            .max_events = options.max_events,
            .timeout = options.timeout
        });

        auto exception = std::exception_ptr();

        wait_for_signal(options.signals);
        async_entry(std::forward<ext::task<>>(task), exception);

        rt.run();

        if (exception) std::rethrow_exception(exception);
    }

    auto yield() -> ext::task<> {
        class awaitable {
            detail::awaiter awaiter;
        public:
            auto await_ready() const noexcept -> bool { return false; }

            auto await_suspend(std::coroutine_handle<> coroutine) -> void {
                awaiter.coroutine = coroutine;
                runtime::current().enqueue(awaiter);
            }

            auto await_resume() -> void {}
        };

        co_await awaitable();
    }
}
