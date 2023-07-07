#pragma once

#include <condition_variable>
#include <ext/coroutine>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

namespace netcore {
    class thread_pool {
        using job_type = std::function<void()>;

        std::queue<job_type> jobs;
        std::vector<std::jthread> threads;
        std::condition_variable condition;
        std::mutex mutex;

        auto entry(std::stop_token stoken) -> void;
    public:
        thread_pool(
            std::string_view name,
            std::size_t size,
            std::size_t backlog_scale_factor = 1
        );

        ~thread_pool();

        auto run(job_type&& job) -> void;
    };
}
