#pragma once

#include "connect.hpp"
#include "endpoint.hpp"
#include "socket.h"

#include <ext/pool>

namespace netcore {
    template <std::constructible_from<socket> T>
    class client {
        class provider {
            endpoint endpoint;
        public:
            provider() = default;

            provider(std::string_view endpoint) :
                endpoint(parse_endpoint(endpoint))
            {}

            auto provide() -> ext::task<T> {
                co_return T{co_await netcore::connect(endpoint)};
            }
        };

        ext::async_pool<T, provider> pool;
    public:
        client() = default;

        client(std::string_view endpoint) : pool(provider(endpoint)) {}

        auto connect() -> ext::task<ext::pool_item<T>> {
            return pool.checkout();
        }
    };
}
