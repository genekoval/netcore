#pragma once

#include "server.hpp"

#include <ext/dynarray>

namespace netcore {
    template <server_context Context>
    class server_list final {
        using endpoint_ref =
            std::optional<std::reference_wrapper<const netcore::endpoint>>;

        struct entry {
            netcore::server<Context> server;
            ext::jtask<> task;

            template <typename Factory, typename Endpoint>
            entry(
                Factory& factory,
                const Endpoint& config,
                endpoint_ref& out
            ) :
                server(factory(config, out))
            {}
        };

        using entry_list = ext::dynarray<entry>;

        entry_list entries;

        server_list(typename entry_list::size_type count) : entries(count) {}
    public:
        template <
            typename Factory,
            typename Endpoint,
            typename ErrorHandler
        >
        requires
            requires(
                Factory factory,
                Endpoint& endpoint,
                endpoint_ref& out
            ) {
                { factory(endpoint, out) } -> std::same_as<server<Context>>;
            } &&
            requires(
                ErrorHandler& on_error,
                Endpoint& endpoint,
                std::exception_ptr ex
            ) {
                { on_error(endpoint, ex) };
            }
        static auto listen(
            std::span<Endpoint> configs,
            Factory&& factory,
            ErrorHandler&& on_error
        ) -> ext::task<server_list> {
            auto list = server_list(configs.size());
            auto& entries = list.entries;

            for (auto& config : configs) {
                auto endpoint = endpoint_ref();
                auto& [server, task] = entries.emplace_back(
                    factory,
                    config,
                    endpoint
                );

                if (!endpoint) throw std::runtime_error("Missing endpoint");
                task = server.listen(endpoint->get());

                if (server.listening()) continue;

                try {
                    co_await task;
                }
                catch (...) {
                    on_error(config, std::current_exception());
                }

                entries.pop_back();
            }

            if (entries.empty()) throw std::runtime_error(
                "Failed to listen for connections"
            );

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
