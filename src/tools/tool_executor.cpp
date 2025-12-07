#include "gpagent/tools/tool_executor.hpp"

namespace gpagent::tools {

// ThreadPool
ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });

                    if (stop_ && tasks_.empty()) {
                        return;
                    }

                    task = std::move(tasks_.front());
                    tasks_.pop();
                }

                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }

    condition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// ToolExecutor
ToolExecutor::ToolExecutor(ToolRegistry& registry, const ConcurrencyConfig& config)
    : registry_(registry)
    , config_(config)
{
    pool_ = std::make_unique<ThreadPool>(config.thread_pool_size);
}

ToolExecutor::~ToolExecutor() = default;

Result<ToolResult, Error> ToolExecutor::execute(const ToolCall& call, const ToolContext& ctx) {
    auto start = std::chrono::steady_clock::now();

    auto result = registry_.execute(call.tool_name, call.arguments, ctx);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<Duration>(end - start);

    if (result.is_ok()) {
        auto& res = result.value();
        res.tool_call_id = call.id;
        record_execution(res.success, duration);
    } else {
        record_execution(false, duration);
    }

    return result;
}

std::vector<ToolResult> ToolExecutor::execute_batch(const std::vector<ToolCall>& calls,
                                                      const ToolContext& ctx) {
    if (calls.empty()) {
        return {};
    }

    // Limit parallel execution
    size_t max_parallel = std::min(
        static_cast<size_t>(config_.max_parallel_tools),
        calls.size()
    );

    std::vector<std::future<ToolResult>> futures;
    futures.reserve(calls.size());

    for (const auto& call : calls) {
        auto future = pool_->submit([this, call, ctx]() -> ToolResult {
            auto result = execute(call, ctx);
            if (result.is_ok()) {
                return std::move(result).value();
            } else {
                return ToolResult{
                    .tool_call_id = call.id,
                    .success = false,
                    .content = "",
                    .error_message = result.error().full_message()
                };
            }
        });
        futures.push_back(std::move(future));
    }

    // Collect results in order
    std::vector<ToolResult> results;
    results.reserve(calls.size());

    for (auto& future : futures) {
        results.push_back(future.get());
    }

    return results;
}

Result<ToolResult, Error> ToolExecutor::execute_with_timeout(const ToolCall& call,
                                                               const ToolContext& ctx,
                                                               Duration timeout) {
    auto future = pool_->submit([this, call, ctx]() {
        return execute(call, ctx);
    });

    // Wait with timeout
    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.timeouts++;
            stats_.total_executions++;
        }

        return Result<ToolResult, Error>::err(
            ErrorCode::ToolTimeout,
            "Tool execution timed out",
            call.tool_name
        );
    }

    return future.get();
}

bool ToolExecutor::requires_confirmation(const ToolId& tool_id) const {
    auto spec = registry_.get_spec(tool_id);
    return spec && spec->requires_confirmation;
}

ToolExecutor::Stats ToolExecutor::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void ToolExecutor::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Stats{};
}

void ToolExecutor::record_execution(bool success, Duration time) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_executions++;
    if (success) {
        stats_.successful++;
    } else {
        stats_.failed++;
    }
    stats_.total_time += time;
}

}  // namespace gpagent::tools
