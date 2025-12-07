#pragma once

#include "gpagent/core/result.hpp"
#include "gpagent/core/types.hpp"
#include "gpagent/tools/tool_executor.hpp"
#include "gpagent/tools/tool_registry.hpp"

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace gpagent::agent {

using namespace gpagent::core;

// Execution result with timing and metadata
struct ExecutionResult {
    std::string tool_name;
    Json arguments;
    std::string output;
    bool success;
    std::chrono::milliseconds duration;
    std::optional<Error> error;
};

// Callback for execution progress
using ExecutionProgressCallback = std::function<void(
    const std::string& tool_name,
    const std::string& status
)>;

// Executor - handles tool execution with validation and error handling
class Executor {
public:
    Executor(tools::ToolRegistry& registry, tools::ToolExecutor& executor);

    // Execute a single tool call
    Result<ExecutionResult, Error> execute(
        const ToolCall& call,
        const tools::ToolContext& context,
        ExecutionProgressCallback progress_cb = nullptr
    );

    // Execute multiple tool calls (potentially in parallel)
    std::vector<ExecutionResult> execute_batch(
        const std::vector<ToolCall>& calls,
        const tools::ToolContext& context,
        ExecutionProgressCallback progress_cb = nullptr
    );

    // Validate tool call before execution
    Result<void, Error> validate(const ToolCall& call) const;

    // Check if tool exists and is enabled
    bool can_execute(const std::string& tool_name) const;

    // Get execution statistics
    struct Stats {
        int total_executions = 0;
        int successful = 0;
        int failed = 0;
        std::chrono::milliseconds total_time{0};
        std::chrono::milliseconds avg_time{0};
    };
    Stats stats() const { return stats_; }

    // Reset statistics
    void reset_stats();

private:
    tools::ToolRegistry& registry_;
    tools::ToolExecutor& executor_;
    Stats stats_;

    void update_stats(const ExecutionResult& result);
};

}  // namespace gpagent::agent
