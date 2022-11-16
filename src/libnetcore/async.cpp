#include <netcore/async.hpp>
#include <netcore/detail/notifier.hpp>
#include <netcore/except.hpp>
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

    inline auto notifier() -> netcore::detail::notifier& {
        return netcore::detail::notifier::instance();
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

        notifier().stop();
    }

    auto wait_for_signal(std::span<const int> signals) -> ext::detached_task {
        auto sig = netcore::signalfd::create(signals);
        auto signal = 0;

        while (true) {
            signal = co_await sig.wait_for_signal();
            if (signal <= 0) break;

            TIMBER_INFO("signal ({}): {}", signal, strsignal(signal));

            if (notifier().shutting_down()) notifier().stop();
            else notifier().shutdown();

            if (notifier().one_or_empty()) break;
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
        auto notifier = detail::notifier(detail::notifier_options {
            .max_events = options.max_events,
            .timeout = options.timeout
        });

        auto exception = std::exception_ptr();

        wait_for_signal(options.signals);
        async_entry(std::forward<ext::task<>>(task), exception);

        notifier.run();

        if (exception) std::rethrow_exception(exception);
    }

    auto yield() -> ext::task<> {
        class awaitable {
            detail::awaiter awaiter;
        public:
            auto await_ready() const noexcept -> bool { return false; }

            auto await_suspend(std::coroutine_handle<> coroutine) -> void {
                awaiter.coroutine = coroutine;
                detail::notifier::instance().enqueue(awaiter);
            }

            auto await_resume() -> void {}
        };

        co_await awaitable();
    }
}
