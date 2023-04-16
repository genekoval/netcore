#pragma once

#include "server.hpp"

#include <ext/dynarray>

namespace netcore {
    template <server_context Context>
    class server_list final {
        struct entry {
            netcore::server<Context> server;
            ext::jtask<> task;

            template <typename Factory>
            entry(Factory& factory) : server(factory()) {}
        };

        ext::dynarray<entry> entries;

        server_list(typename decltype(entries)::size_type count) :
            entries(count)
        {}
    public:
        template <typename Factory, typename ErrorHandler>
        requires
            std::invocable<Factory&> &&
            std::same_as<std::invoke_result_t<Factory&>, server<Context>> &&
            requires(
                ErrorHandler& on_error,
                const endpoint& endpoint,
                std::exception_ptr ex
            ) {
                { on_error(endpoint, ex) };
            }
        static auto listen(
            std::span<const endpoint> endpoints,
            Factory&& factory,
            ErrorHandler&& on_error,
            int backlog = SOMAXCONN
        ) -> ext::task<server_list> {
            auto list = server_list(endpoints.size());
            auto& entries = list.entries;

            for (const auto& endpoint : endpoints) {
                auto& [server, task] = entries.emplace_back(factory);
                task = server.listen(endpoint, backlog);

                if (server.listening()) continue;

                try {
                    co_await task;
                }
                catch (...) {
                    on_error(endpoint, std::current_exception());
                }

                entries.pop_back();
            }

            co_return list;
        }

        auto close() noexcept -> void {
            for (auto& entry : entries) entry.server.close();
        }

        auto connections() const noexcept -> unsigned int {
            unsigned int count = 0;

            for (const auto& entry : entries) {
                count += entry.server.connections();
            }

            return count;
        }

        auto join() -> ext::task<> {
            for (auto&& entry : entries) co_await std::move(entry.task);
        }

        auto listening() const noexcept -> std::size_t {
            std::size_t result = 0;

            for (const auto& entry : entries) {
                if (entry.server.listening()) ++result;
            }

            return result;
        }
    };
}
