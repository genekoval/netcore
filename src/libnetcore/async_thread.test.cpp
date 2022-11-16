#include <netcore/async_thread.hpp>

#include <gtest/gtest.h>

TEST(AsyncThread, Run) {
    auto thread = netcore::async_thread(8, "thread");

    thread.run([](auto id) -> ext::task<> {
        EXPECT_EQ(std::this_thread::get_id(), id);
        co_return;
    }(thread.id()));
}

TEST(AsyncThread, Wait) {
    auto waiter = netcore::async_thread(8, "waiter");

    waiter.run([]() -> ext::task<> {
        auto worker = netcore::async_thread(8, "worker");

        const auto id =
            co_await worker.wait([]() -> ext::task<std::thread::id> {
                co_return std::this_thread::get_id();
            }());

        EXPECT_EQ(worker.id(), id);
    }());
}
