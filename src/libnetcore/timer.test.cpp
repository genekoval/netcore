#include <netcore/event.hpp>
#include <netcore/except.hpp>
#include <netcore/runtime.hpp>
#include <netcore/timer.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

using std::chrono::milliseconds;

namespace {
    auto now() { return std::chrono::steady_clock::now(); }

    auto wait(milliseconds time) -> ext::task<> {
        auto timer = netcore::timer::monotonic();
        timer.set(time);

        const auto start = now();
        const auto expirations = co_await timer.wait();
        const auto elapsed = now() - start;

        EXPECT_EQ(1, expirations);
        EXPECT_GE(elapsed, time);
    }
}

TEST(Timer, WaitUnderSecond) {
    netcore::run([]() -> ext::task<> { co_await wait(100ms); }());
}

TEST(Timer, WaitSecond) {
    netcore::run([]() -> ext::task<> { co_await wait(1s); }());
}

TEST(Timer, Disarm) {
    netcore::run([]() -> ext::task<> {
        constexpr auto time = 30s;

        auto continuation = ext::continuation<>();
        auto canceled = false;
        auto timer = netcore::timer::monotonic();

        const auto wait = [&]() -> ext::detached_task {
            canceled = co_await timer.wait() == 0;
            continuation.resume();
        };

        timer.set(time);
        wait();
        timer.disarm();

        const auto start = now();
        co_await continuation;
        const auto elapsed = now() - start;

        EXPECT_LT(elapsed, time);
        EXPECT_TRUE(canceled);
    }());
}
