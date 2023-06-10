#pragma once

#include "buffered_socket.hpp"

#include <ext/pool>

namespace netcore {
    template <std::constructible_from<buffered_socket> T>
    requires requires(T t) {
        { t.connected() } -> std::same_as<bool>;
        { t.failed() } -> std::same_as<bool>;
    }
    struct client {
        class provider {
            netcore::endpoint endpoint;
            std::size_t buffer_size = 0;
        public:
            provider() = default;

            provider(std::string_view endpoint, std::size_t buffer_size) :
                endpoint(parse_endpoint(endpoint)),
                buffer_size(buffer_size)
            {}

            auto checkin(T& t) -> bool {
                return !t.failed();
            }

            auto checkout(T& t) -> bool {
                return t.connected();
            }

            auto provide() -> ext::task<T> {
                co_return T {
                    co_await buffered_socket::connect(endpoint, buffer_size)
                };
            }
        };

        using pool = ext::pool<provider>;

        client() = default;

        client(std::string_view endpoint) :
            storage(ext::pool_options(), endpoint, 8192)
        {}

        client(
            std::string_view endpoint,
            std::size_t buffer_size,
            const ext::pool_options& options = {}
        ) :
            storage(options, endpoint, buffer_size)
        {}

        auto connect() -> ext::task<typename pool::item> {
            return storage.checkout();
        }
    private:
        pool storage;
    };
}
