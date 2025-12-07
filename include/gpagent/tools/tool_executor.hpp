#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "gpagent/core/types.hpp"
#include "tool_registry.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace gpagent::tools {

using namespace gpagent::core;

// Thread pool for parallel tool execution
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Submit a task and get a future
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    // Get number of threads
    size_t size() const { return workers_.size(); }

    // Shutdown the pool
    void shutdown();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
};

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        tasks_.emplace([task]() { (*task)(); });
    }

    condition_.notify_one();
    return result;
}

// Tool execution request
struct ExecutionRequest {
    ToolCall call;
    ToolContext context;
    std::promise<ToolResult> promise;
};

// Tool executor - handles parallel and async tool execution
class ToolExecutor {
public:
    ToolExecutor(ToolRegistry& registry, const ConcurrencyConfig& config);
    ~ToolExecutor();

    // Execute a single tool call
    Result<ToolResult, Error> execute(const ToolCall& call, const ToolContext& ctx);

    // Execute multiple tool calls (independent ones in parallel)
    std::vector<ToolResult> execute_batch(const std::vector<ToolCall>& calls,
                                           const ToolContext& ctx);

    // Execute with timeout
    Result<ToolResult, Error> execute_with_timeout(const ToolCall& call,
                                                     const ToolContext& ctx,
                                                     Duration timeout);

    // Check if tool requires confirmation
    bool requires_confirmation(const ToolId& tool_id) const;

    // Get execution statistics
    struct Stats {
        int total_executions = 0;
        int successful = 0;
        int failed = 0;
        int timeouts = 0;
        Duration total_time{0};
    };
    Stats get_stats() const;
    void reset_stats();

private:
    ToolRegistry& registry_;
    std::unique_ptr<ThreadPool> pool_;
    ConcurrencyConfig config_;

    mutable std::mutex stats_mutex_;
    Stats stats_;

    void record_execution(bool success, Duration time);
};

}  // namespace gpagent::tools
