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
}

TEST(Timer, Wait) {
    netcore::async([]() -> ext::task<> {
        constexpr auto wait = 200ms;

        auto timer = netcore::timer::monotonic();
        timer.set(wait);

        const auto start = now();
        co_await timer.wait();
        const auto end = now();

        const auto elapsed = duration_cast<milliseconds>(end - start);

        EXPECT_GE(elapsed.count(), wait.count());
    }());
}

TEST(Timer, Disarm) {
    netcore::async([]() -> ext::task<> {
        constexpr auto wait = 200ms;

        const auto task = [](
            netcore::timer& timer,
            bool& canceled,
            netcore::event& event
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
        timer.set(wait);

        task(timer, canceled, event);
        timer.disarm();

        const auto start = now();
        co_await event.listen();
        const auto end = now();

        const auto elapsed = duration_cast<milliseconds>(end - start);

        EXPECT_LT(elapsed.count(), wait.count());
        EXPECT_TRUE(canceled);
    }());
}
