#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "tool_spec.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gpagent::tools {

using namespace gpagent::core;

// Tool registration entry
struct RegisteredTool {
    ToolSpec spec;
    ToolHandler handler;
    bool enabled = true;
    std::string source;  // "builtin", "plugin", "mcp:server_name"
};

// Tool registry - manages all available tools
class ToolRegistry {
public:
    ToolRegistry() = default;
    explicit ToolRegistry(const ToolsConfig& config);

    // Register a tool
    Result<void, Error> register_tool(const ToolSpec& spec, ToolHandler handler,
                                       const std::string& source = "builtin");

    // Unregister a tool
    Result<void, Error> unregister_tool(const ToolId& id);

    // Check if tool exists
    bool has_tool(const ToolId& id) const;

    // Get tool spec
    std::optional<ToolSpec> get_spec(const ToolId& id) const;

    // Get all tool specs (for sending to LLM)
    std::vector<ToolSpec> get_all_specs() const;

    // Get enabled tool specs only
    std::vector<ToolSpec> get_enabled_specs() const;

    // Convert to LLM format
    Json to_claude_format() const;
    Json to_gemini_format() const;

    // Enable/disable tools
    Result<void, Error> enable_tool(const ToolId& id);
    Result<void, Error> disable_tool(const ToolId& id);
    bool is_enabled(const ToolId& id) const;

    // Execute a tool
    Result<ToolResult, Error> execute(const ToolId& id, const Json& args,
                                       const ToolContext& ctx);

    // Search for tools by keywords
    std::vector<ToolSpec> search(const std::string& query) const;

    // Get tool count
    size_t size() const;

    // Register all builtin tools
    void register_builtins();
    void register_builtin_tools() { register_builtins(); }

    // Get tool spec (alias for get_spec)
    std::optional<ToolSpec> get_tool(const ToolId& id) const { return get_spec(id); }

    // Get all registered tools
    const std::unordered_map<ToolId, RegisteredTool>& all_tools() const { return tools_; }

private:
    mutable std::mutex mutex_;
    std::unordered_map<ToolId, RegisteredTool> tools_;
    ToolsConfig config_;

    // Validate tool arguments against spec
    Result<void, Error> validate_args(const ToolSpec& spec, const Json& args) const;
};

}  // namespace gpagent::tools
