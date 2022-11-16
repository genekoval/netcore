#include <netcore/netcore>

#include <gtest/gtest.h>
#include <timber/timber>

namespace fs = std::filesystem;

using type = netcore::fork_server::process_type;

constexpr auto socket_name = "netcore.sock";

const auto unix_socket = netcore::unix_socket {
    .path = fs::path(TESTDIR) / socket_name,
    .mode = fs::perms::owner_read | fs::perms::owner_write
};

const auto increment = [](auto&& socket) -> ext::task<> {
    auto number = std::int32_t();
    co_await socket.read(&number, sizeof(number));

    TIMBER_DEBUG("increment: received {}", number);

    number++;
    co_await socket.write(&number, sizeof(number));
};

TEST(ServerTest, StartStop) {
    auto server = netcore::fork_server(increment);

    if (type::server == server.start(
        unix_socket,
        []() {
            TIMBER_INFO("Listening for connections");
        }
    )) { std::exit(0); }

    ASSERT_TRUE(std::filesystem::is_socket(unix_socket.path));

    const auto code = server.stop();
    ASSERT_EQ(0, code);
}

TEST(ServerTest, Connection) {
    constexpr auto number = 3;

    auto server = netcore::fork_server(increment);

    if (type::server == server.start(
        unix_socket,
        []() {
            TIMBER_INFO("Listening for connections");
        }
    )) { std::exit(0); }

    const auto task = [number]() -> ext::task<> {
        auto client = co_await netcore::connect(
            unix_socket.path.string()
        );

        co_await client.write(&number, sizeof(number));

        auto result = decltype(number)(0);
        co_await client.read(&result, sizeof(result));

        EXPECT_EQ(number + 1, result);
    };

    netcore::async(task());

    server.stop();
}
