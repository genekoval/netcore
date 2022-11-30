#include <netcore/async.hpp>
#include <netcore/event.hpp>

#include <gtest/gtest.h>

TEST(Event, Void) {
    netcore::async([]() -> ext::task<> {
        auto event = netcore::event();
        auto emitted = false;

        [](netcore::event<>& event, bool& emitted) -> ext::detached_task {
            co_await event.listen();
            emitted = true;
            event.emit();
        }(event, emitted);

        event.emit();
        co_await event.listen();

        EXPECT_TRUE(emitted);
    }());
}

TEST(Event, Value) {
    netcore::async([]() -> ext::task<> {
        auto event = netcore::event<int>();

        [](netcore::event<int>& event) -> ext::detached_task {
            const auto n = co_await event.listen();
            EXPECT_EQ(1, n);

            event.emit(2);
        }(event);

        event.emit(1);

        const auto n = co_await event.listen();
        EXPECT_EQ(2, n);
    }());
}