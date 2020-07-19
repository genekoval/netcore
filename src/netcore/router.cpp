#include "router.h"

#include <functional>
#include <nlohmann/json.hpp>
#include <timber/timber>
#include <unistd.h>
#include <unordered_map>

using json = nlohmann::json;

constexpr auto indent = 4;

namespace netcore::cli {
    auto echo(const json& data) -> json {
        auto message = data.get<std::string>();
        INFO() << "Client sent: " << message;
        return message;
    }

    auto pid(const json& data) -> json {
        return getpid();
    }

    using route_handler = std::function<json(const json&)>;

    const std::unordered_map<std::string, route_handler> router = {
        {"echo", echo},
        {"pid", pid}
    };

    auto send(
        const socket& client,
        std::string_view event,
        const json& data
    ) -> void {
        auto package = json();

        package["event"] = event;
        package["data"] = data;

        DEBUG() << "Response: " << package.dump(indent);
        client.send(package.dump());
    }

    auto route(socket&& client) -> void {
        auto request = json::parse(client.recv());

        DEBUG() << "Received: " << request.dump(indent);

        auto event = request["event"].get<std::string>();

        if (router.contains(event)) {
            INFO() << "Event: " << event;

            auto data = router.at(event)(request["data"]);
            send(client, event, data);
        }
        else {
            auto message = "Unknown event type: " + event;
            INFO() << message;

            send(client, "error", message);
        }
    }
}
