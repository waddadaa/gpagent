#include "gpagent/agent/executor.hpp"

#include <spdlog/spdlog.h>

namespace gpagent::agent {

Executor::Executor(tools::ToolRegistry& registry, tools::ToolExecutor& executor)
    : registry_(registry)
    , executor_(executor)
{
}

Result<ExecutionResult, Error> Executor::execute(
    const ToolCall& call,
    const tools::ToolContext& context,
    ExecutionProgressCallback progress_cb) {

    ExecutionResult result;
    result.tool_name = call.tool_name;
    result.arguments = call.arguments;

    // Validate first
    auto validation = validate(call);
    if (validation.is_err()) {
        result.success = false;
        result.error = validation.error();
        result.output = validation.error().message;
        result.duration = std::chrono::milliseconds(0);
        update_stats(result);
        return Result<ExecutionResult, Error>::err(std::move(validation).error());
    }

    if (progress_cb) {
        progress_cb(call.tool_name, "starting");
    }

    auto start = std::chrono::steady_clock::now();

    // Execute the tool
    auto exec_result = executor_.execute(call, context);

    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (exec_result.is_ok()) {
        result.success = true;
        result.output = exec_result.value().content;

        if (progress_cb) {
            progress_cb(call.tool_name, "completed");
        }
    } else {
        result.success = false;
        result.error = exec_result.error();
        result.output = exec_result.error().message;

        if (progress_cb) {
            progress_cb(call.tool_name, "failed");
        }
    }

    update_stats(result);

    if (result.success) {
        return Result<ExecutionResult, Error>::ok(std::move(result));
    } else {
        return Result<ExecutionResult, Error>::err(*result.error);
    }
}

std::vector<ExecutionResult> Executor::execute_batch(
    const std::vector<ToolCall>& calls,
    const tools::ToolContext& context,
    ExecutionProgressCallback progress_cb) {

    std::vector<ExecutionResult> results;
    results.reserve(calls.size());

    // Check if we can parallelize (tools must not depend on each other)
    // For now, execute sequentially - parallel execution would need dependency analysis

    for (const auto& call : calls) {
        auto result = execute(call, context, progress_cb);

        if (result.is_ok()) {
            results.push_back(std::move(result).value());
        } else {
            ExecutionResult err_result;
            err_result.tool_name = call.tool_name;
            err_result.arguments = call.arguments;
            err_result.success = false;
            err_result.error = result.error();
            err_result.output = result.error().message;
            err_result.duration = std::chrono::milliseconds(0);
            results.push_back(std::move(err_result));
        }
    }

    return results;
}

Result<void, Error> Executor::validate(const ToolCall& call) const {
    // Check if tool exists
    auto spec_opt = registry_.get_tool(call.tool_name);
    if (!spec_opt) {
        return Result<void, Error>::err(
            ErrorCode::ToolNotFound,
            "Tool not found: " + call.tool_name
        );
    }

    // Check if tool is enabled
    if (!registry_.is_enabled(call.tool_name)) {
        return Result<void, Error>::err(
            ErrorCode::ToolDisabled,
            "Tool is disabled: " + call.tool_name
        );
    }

    const auto& spec = *spec_opt;

    // Validate required parameters
    for (const auto& param : spec.parameters) {
        if (param.required && !call.arguments.contains(param.name)) {
            return Result<void, Error>::err(
                ErrorCode::ToolValidationFailed,
                "Missing required parameter: " + param.name + " for tool " + call.tool_name
            );
        }
    }

    // Type validation for provided parameters
    for (const auto& [key, value] : call.arguments.items()) {
        // Find the parameter spec
        const tools::ParamSpec* param_spec = nullptr;
        for (const auto& param : spec.parameters) {
            if (param.name == key) {
                param_spec = &param;
                break;
            }
        }

        if (!param_spec) {
            // Unknown parameter - could be lenient or strict
            spdlog::warn("Unknown parameter {} for tool {}", key, call.tool_name);
            continue;
        }

        // Basic type checking
        if (param_spec->type == tools::ParamType::String && !value.is_string()) {
            return Result<void, Error>::err(
                ErrorCode::ToolValidationFailed,
                "Parameter " + key + " should be string"
            );
        }
        if (param_spec->type == tools::ParamType::Integer && !value.is_number_integer()) {
            return Result<void, Error>::err(
                ErrorCode::ToolValidationFailed,
                "Parameter " + key + " should be integer"
            );
        }
        if (param_spec->type == tools::ParamType::Boolean && !value.is_boolean()) {
            return Result<void, Error>::err(
                ErrorCode::ToolValidationFailed,
                "Parameter " + key + " should be boolean"
            );
        }
        if (param_spec->type == tools::ParamType::Array && !value.is_array()) {
            return Result<void, Error>::err(
                ErrorCode::ToolValidationFailed,
                "Parameter " + key + " should be array"
            );
        }
        if (param_spec->type == tools::ParamType::Object && !value.is_object()) {
            return Result<void, Error>::err(
                ErrorCode::ToolValidationFailed,
                "Parameter " + key + " should be object"
            );
        }

        // Enum validation
        if (param_spec->enum_values && !param_spec->enum_values->empty() && value.is_string()) {
            std::string str_val = value.get<std::string>();
            bool valid = std::find(param_spec->enum_values->begin(),
                                    param_spec->enum_values->end(),
                                    str_val) != param_spec->enum_values->end();
            if (!valid) {
                return Result<void, Error>::err(
                    ErrorCode::ToolValidationFailed,
                    "Invalid value for " + key + ": " + str_val
                );
            }
        }
    }

    return Result<void, Error>::ok();
}

bool Executor::can_execute(const std::string& tool_name) const {
    auto spec = registry_.get_tool(tool_name);
    return spec.has_value() && registry_.is_enabled(tool_name);
}

void Executor::reset_stats() {
    stats_ = Stats{};
}

void Executor::update_stats(const ExecutionResult& result) {
    ++stats_.total_executions;

    if (result.success) {
        ++stats_.successful;
    } else {
        ++stats_.failed;
    }

    stats_.total_time += result.duration;

    if (stats_.total_executions > 0) {
        stats_.avg_time = std::chrono::milliseconds(
            stats_.total_time.count() / stats_.total_executions
        );
    }
}

}  // namespace gpagent::agent
