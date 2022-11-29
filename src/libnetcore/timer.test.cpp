#include <netcore/async.hpp>
#include <netcore/event.hpp>
#include <netcore/except.hpp>
#include <netcore/timer.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

using std::chrono::duration_cast;
using std::chrono::milliseconds;

namespace {
    auto now() {
        return std::chrono::steady_clock::now();
    }

    auto wait(milliseconds time) -> ext::task<> {
        auto timer = netcore::timer::monotonic();
        timer.set(time);

        const auto start = now();
        co_await timer.wait();
        const auto end = now();

        const auto elapsed = duration_cast<milliseconds>(end - start);

        EXPECT_GE(elapsed, time);
    }
}

TEST(Timer, WaitUnderSecond) {
    netcore::async([]() -> ext::task<> {
        co_await wait(100ms);
    }());
}

TEST(Timer, WaitSecond) {
    netcore::async([]() -> ext::task<> {
        co_await wait(1s);
    }());
}

TEST(Timer, Disarm) {
    netcore::async([]() -> ext::task<> {
        constexpr auto time = 200ms;

        const auto task = [](
            netcore::timer& timer,
            bool& canceled,
            netcore::event<>& event
        ) -> ext::detached_task {
            try {
                co_await timer.wait();
            }
            catch (const netcore::task_canceled&) {
                canceled = true;
            }

            event.emit();
        };

        auto event = netcore::event();
        auto canceled = false;
        auto timer = netcore::timer::monotonic();
        timer.set(time);

        task(timer, canceled, event);
        timer.disarm();

        const auto start = now();
        co_await event.listen();
        const auto end = now();

        const auto elapsed = duration_cast<milliseconds>(end - start);

        EXPECT_LT(elapsed, time);
        EXPECT_TRUE(canceled);
    }());
}
