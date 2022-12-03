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
        { f(std::forward<netcore::socket>(client)) } ->
            std::same_as<ext::task<>>;
    };

    const auto unix_socket = netcore::unix_socket {
        .path = fs::temp_directory_path() / socket_name,
        .mode = fs::perms::owner_read | fs::perms::owner_write
    };

    struct server_context {
        netcore::event<> closed;

        auto close() -> void {
            TIMBER_INFO("Test server closed");
            closed.emit();
        }

        auto connection(netcore::socket&& client) -> ext::task<> {
            number_type number = 0;
            co_await client.read(&number, sizeof(number_type));

            TIMBER_DEBUG("increment: received {}", number);

            ++number;
            co_await client.write(&number, sizeof(number_type));
        }

        auto listen() -> void {
            TIMBER_INFO("Test server listening for connections");
        }
    };
}

class ServerTest : public Test {
    auto close() -> ext::task<> {
        return server.context.closed.listen();
    }

    auto connect() -> ext::task<netcore::socket> {
        co_return co_await netcore::connect(unix_socket.path.string());
    }

    auto listen() -> ext::detached_task {
        co_await server.listen(unix_socket);
    }

    auto task(const client_handler auto& handler) -> ext::task<> {
        listen();

        co_await handler(co_await connect());

        server.close();
        co_await close();
    }
protected:
    netcore::server<server_context> server;

    ServerTest() : server() {}

    auto connect(const client_handler auto& handler) -> void {
        netcore::run(task(handler));
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
