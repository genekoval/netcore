#include <netcore/mutex.hpp>
#include <netcore/runtime.hpp>

#include <gtest/gtest.h>

class Mutex : public testing::Test {
protected:
    netcore::mutex<int> mutex = 0;

    auto increment() -> ext::detached_task { ++*co_await mutex.lock(); }
};

TEST_F(Mutex, Get) {
    auto& a = mutex.get();
    auto& b = mutex.get();

    ++a;
    ++b;

    EXPECT_EQ(2, mutex.get());
}

TEST_F(Mutex, Lock) {
    netcore::run([&]() -> ext::task<> {
        {
            auto lock = co_await mutex.lock();
            increment();

            EXPECT_EQ(0, *lock);

            co_await netcore::yield();

            EXPECT_EQ(0, *lock);
        }

        auto lock = co_await mutex.lock();
        EXPECT_EQ(1, *lock);
    }());
}

TEST_F(Mutex, MultipleAwaiters) {
    netcore::run([&]() -> ext::task<> {
        {
            const auto lock = co_await mutex.lock();

            for (auto i = 0; i < 10; ++i) increment();

            EXPECT_EQ(0, *lock);
        }

        const auto lock = co_await mutex.lock();
        EXPECT_EQ(10, *lock);
    }());
}
