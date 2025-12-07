#include "gpagent/tools/tool_registry.hpp"

#include <algorithm>
#include <sstream>

namespace gpagent::tools {

ToolRegistry::ToolRegistry(const ToolsConfig& config)
    : config_(config)
{
}

Result<void, Error> ToolRegistry::register_tool(const ToolSpec& spec, ToolHandler handler,
                                                  const std::string& source) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (tools_.count(spec.name)) {
        return Result<void, Error>::err(
            ErrorCode::AlreadyExists,
            "Tool already registered",
            spec.name
        );
    }

    RegisteredTool tool;
    tool.spec = spec;
    tool.handler = std::move(handler);
    tool.source = source;

    // Check if tool is enabled in config
    auto it = config_.builtin.find(spec.name);
    if (it != config_.builtin.end()) {
        tool.enabled = it->second.enabled;
        tool.spec.timeout_ms = it->second.timeout_ms;
        tool.spec.requires_confirmation = it->second.require_confirm;
    } else {
        tool.enabled = true;
    }

    tools_[spec.name] = std::move(tool);
    return Result<void, Error>::ok();
}

Result<void, Error> ToolRegistry::unregister_tool(const ToolId& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!tools_.count(id)) {
        return Result<void, Error>::err(
            ErrorCode::ToolNotFound,
            "Tool not found",
            id
        );
    }

    tools_.erase(id);
    return Result<void, Error>::ok();
}

bool ToolRegistry::has_tool(const ToolId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.count(id) > 0;
}

std::optional<ToolSpec> ToolRegistry::get_spec(const ToolId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tools_.find(id);
    if (it == tools_.end()) {
        return std::nullopt;
    }

    return it->second.spec;
}

std::vector<ToolSpec> ToolRegistry::get_all_specs() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ToolSpec> specs;
    specs.reserve(tools_.size());

    for (const auto& [id, tool] : tools_) {
        specs.push_back(tool.spec);
    }

    return specs;
}

std::vector<ToolSpec> ToolRegistry::get_enabled_specs() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ToolSpec> specs;

    for (const auto& [id, tool] : tools_) {
        if (tool.enabled) {
            specs.push_back(tool.spec);
        }
    }

    return specs;
}

Json ToolRegistry::to_claude_format() const {
    Json tools = Json::array();

    for (const auto& spec : get_enabled_specs()) {
        tools.push_back(spec.to_claude_format());
    }

    return tools;
}

Json ToolRegistry::to_gemini_format() const {
    Json tools = Json::array();

    for (const auto& spec : get_enabled_specs()) {
        tools.push_back(spec.to_gemini_format());
    }

    return Json{{"function_declarations", tools}};
}

Result<void, Error> ToolRegistry::enable_tool(const ToolId& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tools_.find(id);
    if (it == tools_.end()) {
        return Result<void, Error>::err(ErrorCode::ToolNotFound, "Tool not found", id);
    }

    it->second.enabled = true;
    return Result<void, Error>::ok();
}

Result<void, Error> ToolRegistry::disable_tool(const ToolId& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tools_.find(id);
    if (it == tools_.end()) {
        return Result<void, Error>::err(ErrorCode::ToolNotFound, "Tool not found", id);
    }

    it->second.enabled = false;
    return Result<void, Error>::ok();
}

bool ToolRegistry::is_enabled(const ToolId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tools_.find(id);
    return it != tools_.end() && it->second.enabled;
}

Result<void, Error> ToolRegistry::validate_args(const ToolSpec& spec, const Json& args) const {
    // Check required parameters
    for (const auto& param : spec.parameters) {
        if (param.required && !args.contains(param.name)) {
            return Result<void, Error>::err(
                ErrorCode::ToolValidationFailed,
                "Missing required parameter: " + param.name,
                spec.name
            );
        }
    }

    // Validate parameter types
    for (const auto& param : spec.parameters) {
        if (!args.contains(param.name)) continue;

        const auto& value = args[param.name];
        bool valid = true;

        switch (param.type) {
            case ParamType::String:
                valid = value.is_string();
                break;
            case ParamType::Integer:
                valid = value.is_number_integer();
                break;
            case ParamType::Number:
                valid = value.is_number();
                break;
            case ParamType::Boolean:
                valid = value.is_boolean();
                break;
            case ParamType::Array:
                valid = value.is_array();
                break;
            case ParamType::Object:
                valid = value.is_object();
                break;
        }

        if (!valid) {
            return Result<void, Error>::err(
                ErrorCode::ToolValidationFailed,
                "Invalid type for parameter: " + param.name,
                spec.name
            );
        }

        // Check enum values
        if (param.enum_values && value.is_string()) {
            const auto& enum_vals = *param.enum_values;
            const auto& str_value = value.get<std::string>();
            if (std::find(enum_vals.begin(), enum_vals.end(), str_value) == enum_vals.end()) {
                return Result<void, Error>::err(
                    ErrorCode::ToolValidationFailed,
                    "Invalid enum value for parameter: " + param.name,
                    spec.name
                );
            }
        }
    }

    return Result<void, Error>::ok();
}

Result<ToolResult, Error> ToolRegistry::execute(const ToolId& id, const Json& args,
                                                  const ToolContext& ctx) {
    RegisteredTool tool;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = tools_.find(id);
        if (it == tools_.end()) {
            return Result<ToolResult, Error>::err(
                ErrorCode::ToolNotFound,
                "Tool not found",
                id
            );
        }

        if (!it->second.enabled) {
            return Result<ToolResult, Error>::err(
                ErrorCode::ToolDisabled,
                "Tool is disabled",
                id
            );
        }

        tool = it->second;
    }

    // Validate arguments
    auto validation = validate_args(tool.spec, args);
    if (validation.is_err()) {
        return Result<ToolResult, Error>::err(std::move(validation).error());
    }

    // Execute the tool
    try {
        auto start = std::chrono::steady_clock::now();
        ToolResult result = tool.handler(args, ctx);
        auto end = std::chrono::steady_clock::now();

        result.execution_time = std::chrono::duration_cast<Duration>(end - start);
        return Result<ToolResult, Error>::ok(std::move(result));

    } catch (const std::exception& e) {
        return Result<ToolResult, Error>::err(
            ErrorCode::ToolExecutionFailed,
            e.what(),
            id
        );
    }
}

std::vector<ToolSpec> ToolRegistry::search(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Tokenize query
    std::vector<std::string> query_words;
    std::istringstream iss(query);
    std::string word;
    while (iss >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        query_words.push_back(word);
    }

    std::vector<std::pair<int, ToolSpec>> scored;

    for (const auto& [id, tool] : tools_) {
        if (!tool.enabled) continue;

        int score = 0;

        // Check name
        std::string name_lower = tool.spec.name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        for (const auto& qw : query_words) {
            if (name_lower.find(qw) != std::string::npos) {
                score += 10;
            }
        }

        // Check keywords
        for (const auto& keyword : tool.spec.keywords) {
            std::string kw_lower = keyword;
            std::transform(kw_lower.begin(), kw_lower.end(), kw_lower.begin(), ::tolower);
            for (const auto& qw : query_words) {
                if (kw_lower.find(qw) != std::string::npos) {
                    score += 5;
                }
            }
        }

        // Check description
        std::string desc_lower = tool.spec.description;
        std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(), ::tolower);
        for (const auto& qw : query_words) {
            if (desc_lower.find(qw) != std::string::npos) {
                score += 2;
            }
        }

        if (score > 0) {
            scored.emplace_back(score, tool.spec);
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<ToolSpec> results;
    for (const auto& [_, spec] : scored) {
        results.push_back(spec);
    }

    return results;
}

size_t ToolRegistry::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.size();
}

// Forward declarations for builtin tool registration functions
namespace builtin {
    void register_file_tools(ToolRegistry& registry);
    void register_search_tools(ToolRegistry& registry);
    void register_bash_tool(ToolRegistry& registry);
    void register_web_tools(ToolRegistry& registry);
    void register_git_tools(ToolRegistry& registry);
    void register_memory_tools(ToolRegistry& registry);
    void register_interaction_tools(ToolRegistry& registry);
    void register_code_tools(ToolRegistry& registry);
}

void ToolRegistry::register_builtins() {
    // Register all builtin tools from their respective files
    builtin::register_file_tools(*this);
    builtin::register_search_tools(*this);
    builtin::register_bash_tool(*this);
    builtin::register_web_tools(*this);
    builtin::register_git_tools(*this);
    builtin::register_memory_tools(*this);
    builtin::register_interaction_tools(*this);
    builtin::register_code_tools(*this);
}

}  // namespace gpagent::tools
