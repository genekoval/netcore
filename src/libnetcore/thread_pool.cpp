#include <netcore/eventfd.h>
#include <netcore/thread_pool.hpp>

#include <fmt/format.h>
#include <timber/timber>

using lock = std::unique_lock<std::mutex>;

namespace netcore {
    thread_pool::thread_pool(
        std::string_view name,
        std::size_t size,
        std::size_t backlog_scale_factor
    ) {
        threads.reserve(size);

        for (auto i = 0; i < size; ++i) {
            threads.emplace_back([this, name, i](std::stop_token stoken) {
                timber::thread_name = fmt::format("{}[{}]", name, i);
                entry(stoken);
            });
        }
    }

    thread_pool::~thread_pool() {
        for (auto& thread : threads) thread.request_stop();
        condition.notify_all();
    }

    auto thread_pool::entry(std::stop_token stoken) -> void {
        auto job = job_type();

        while (!stoken.stop_requested()) {
            {
                auto l = lock(mutex);

                condition.wait(l, [this, &stoken]() {
                    return stoken.stop_requested() || !jobs.empty();
                });

                if (jobs.empty()) continue;

                job = std::move(jobs.front());
                jobs.pop();
            }

            job();
        }
    }

    auto thread_pool::run(job_type&& job) -> void {
        jobs.emplace(std::forward<job_type>(job));
        condition.notify_one();
    }
}
