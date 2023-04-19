#include <netcore/async_thread.hpp>
#include <netcore/runtime.hpp>

#include <gtest/gtest.h>
#include <timber/timber>

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

TEST(AsyncThread, WaitException) {
    netcore::run([]() -> ext::task<> {
        auto thread = make_thread();

        EXPECT_THROW(
            co_await thread.wait([]() -> ext::task<std::thread::id> {
                throw std::runtime_error("test error");
                co_return std::this_thread::get_id();
            }()),
            std::runtime_error
        );
    }());
}
