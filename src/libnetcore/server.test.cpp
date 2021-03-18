#include <netcore/client.h>
#include <netcore/server.h>

#include <gtest/gtest.h>
#include <timber/timber>

using type = netcore::fork_server::process_type;

constexpr auto socket_name = "netcore.sock";
const auto socket_path = std::filesystem::path(TESTDIR) / socket_name;

const auto increment = [](auto&& socket) {
    auto number = std::int32_t();
    socket.read(&number, sizeof(number));
    DEBUG() << "increment: received " << number;

    number++;
    socket.write(&number, sizeof(number));
};

TEST(ServerTest, StartStop) {
    auto server = netcore::fork_server(increment);

    if (type::server == server.start(
        socket_path,
        std::filesystem::perms::unknown,
        []() {
            INFO() << "Listening for connections";
        }
    )) {
        ASSERT_FALSE(std::filesystem::exists(socket_path));
        return;
    }

    ASSERT_TRUE(std::filesystem::is_socket(socket_path));

    const auto code = server.stop();
    ASSERT_EQ(0, code);
}

TEST(ServerTest, Connection) {
    constexpr auto number = 3;

    auto server = netcore::fork_server(increment);

    if (type::server == server.start(
        socket_path,
        std::filesystem::perms::unknown,
        []() {
            INFO() << "Listening for connections";
        }
    )) { return; }

    auto client = netcore::connect(socket_path.string());
    auto result = 0;

    client.write(&number, sizeof(number));
    client.read(&result, sizeof(result));

    ASSERT_EQ(number + 1, result);

    server.stop();
}