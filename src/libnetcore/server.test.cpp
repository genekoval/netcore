#include <netcore/netcore>

#include <gtest/gtest.h>
#include <timber/timber>

namespace fs = std::filesystem;

using testing::Test;

namespace {
    using number_type = std::int32_t;

    constexpr auto socket_name = "netcore.test.sock";

    template <typename F>
    concept client_handler = requires(const F& f, netcore::socket&& client) {
        {
            f(std::forward<netcore::socket>(client))
        } -> std::same_as<ext::task<>>;
    };

    const auto unix_socket = netcore::unix_socket {
        .path = fs::temp_directory_path() / socket_name,
        .mode = fs::perms::owner_read | fs::perms::owner_write
    };

    const auto increment = [](auto&& socket) -> ext::task<> {
        number_type number = 0;
        co_await socket.read(&number, sizeof(number_type));

        TIMBER_DEBUG("increment: received {}", number);

        number++;
        co_await socket.write(&number, sizeof(number_type));
    };
}

class ServerTest : public Test {
    auto listen(netcore::event<>& event) -> ext::detached_task {
        co_await server.listen(unix_socket, [] {
            TIMBER_INFO("Test server listening for connections");
        });
    }
protected:
    netcore::server server;

    ServerTest() : server(increment) {}

    auto connect(const client_handler auto& handler) -> void {
        netcore::run([&]() -> ext::task<> {
            netcore::event<> event;

            listen(event);

            auto client = co_await netcore::connect(unix_socket.path.string());
            co_await handler(std::move(client));

            // The handler may not have suspended.
            // Yield to make sure the runtime starts.
            co_await netcore::yield();

            // Stop the server and place the server task in the queue.
            netcore::runtime::current().shutdown();

            // Place this task behind the server task in the queue.
            co_await netcore::yield();
        }());
    }
};

TEST_F(ServerTest, StartStop) {
    connect([&](netcore::socket&& client) -> ext::task<> {
        EXPECT_TRUE(fs::is_socket(unix_socket.path));
        co_return;
    });

    EXPECT_FALSE(fs::exists(unix_socket.path));
}

TEST_F(ServerTest, Connection) {
    connect([](netcore::socket&& client) -> ext::task<> {
        constexpr number_type number = 3;

        co_await client.write(&number, sizeof(number_type));

        number_type result = 0;
        co_await client.read(&result, sizeof(number_type));

        EXPECT_EQ(number + 1, result);
    });
}
