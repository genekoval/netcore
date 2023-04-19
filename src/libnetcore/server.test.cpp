#include <netcore/netcore>

#include <gtest/gtest.h>
#include <timber/timber>

namespace fs = std::filesystem;

using netcore::address_type;
using netcore::unix_socket;
using testing::Test;

namespace {
    using number_type = std::int32_t;

    constexpr auto socket_name = "netcore.test.sock";

    template <typename F>
    concept client_handler = requires(const F& f, netcore::socket&& client) {
        { f(std::forward<netcore::socket>(client)) } ->
            std::same_as<ext::task<>>;
    };

    const netcore::endpoint endpoint = unix_socket {
        .path = fs::temp_directory_path() / socket_name,
        .mode = fs::perms::owner_read | fs::perms::owner_write
    };

    struct server_context {
        auto close() -> void {
            TIMBER_INFO("Test server closed");
        }

        auto connection(netcore::socket&& client) -> ext::task<> {
            number_type number = 0;
            co_await client.read(&number, sizeof(number_type));

            TIMBER_DEBUG("increment: received {}", number);

            ++number;
            co_await client.write(&number, sizeof(number_type));
        }

        auto listen(const address_type& address) -> void {
            TIMBER_INFO(
                "Test server listening for connections on {}",
                address
            );
        }
    };
}

class ServerTest : public Test {
    auto connect() -> ext::task<netcore::socket> {
        co_return co_await netcore::connect(endpoint);
    }

    auto task(const client_handler auto& handler) -> ext::task<> {
        auto server_task = server.listen(endpoint);
        if (server_task.is_ready()) co_await server_task;

        co_await handler(co_await connect());

        server.close();
        co_await server_task;
    }
protected:
    netcore::server<server_context> server;

    ServerTest() : server() {}

    auto connect(const client_handler auto& handler) -> void {
        netcore::run(task(handler));
    }
};

TEST_F(ServerTest, StartStop) {
    const auto& unix = std::get<netcore::unix_socket>(endpoint);

    connect([&](netcore::socket&& client) -> ext::task<> {
        EXPECT_TRUE(fs::is_socket(unix.path));
        co_return;
    });

    EXPECT_FALSE(fs::exists(unix.path));
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

TEST_F(ServerTest, ServerInfo) {
    EXPECT_FALSE(server.listening());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(server.address()));
    EXPECT_EQ(0, server.connections());

    connect([&](netcore::socket&& client) -> ext::task<> {
        EXPECT_TRUE(server.listening());

        EXPECT_TRUE(std::holds_alternative<fs::path>(server.address()));
        if (const auto* const path = std::get_if<fs::path>(&server.address())) {
            EXPECT_EQ(std::get<unix_socket>(endpoint).path, *path);
        }

        co_await netcore::yield();
        EXPECT_EQ(1, server.connections());

        co_return;
    });

    EXPECT_FALSE(server.listening());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(server.address()));
    EXPECT_EQ(0, server.connections());
}
