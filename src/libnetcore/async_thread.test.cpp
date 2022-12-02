#include <netcore/async.hpp>
#include <netcore/async_thread.hpp>

#include <gtest/gtest.h>

namespace {
    auto make_thread() -> netcore::async_thread {
        return netcore::async_thread(8, "thread");
    }
}

TEST(AsyncThread, Run) {
    auto thread = make_thread();

    thread.run([](auto id) -> ext::task<> {
        EXPECT_EQ(std::this_thread::get_id(), id);
        co_return;
    }(thread.id()));
}

TEST(AsyncThread, Wait) {
    netcore::run([]() -> ext::task<> {
        auto thread = make_thread();

        const auto id = co_await thread.wait(
            []() -> ext::task<std::thread::id> {
                co_return std::this_thread::get_id();
            }()
        );

        EXPECT_EQ(thread.id(), id);
    }());
}
