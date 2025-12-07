#pragma once

#include "gpagent/core/types.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

// Forward declaration
namespace gpagent::core {
    struct Config;
}

namespace gpagent::tools {

using namespace gpagent::core;

// Parameter types for tool arguments
enum class ParamType {
    String,
    Integer,
    Number,
    Boolean,
    Array,
    Object
};

inline std::string_view param_type_to_string(ParamType type) {
    switch (type) {
        case ParamType::String: return "string";
        case ParamType::Integer: return "integer";
        case ParamType::Number: return "number";
        case ParamType::Boolean: return "boolean";
        case ParamType::Array: return "array";
        case ParamType::Object: return "object";
    }
    return "string";
}

// Parameter specification
struct ParamSpec {
    std::string name;
    std::string description;
    ParamType type = ParamType::String;
    bool required = false;
    std::optional<Json> default_value;
    std::optional<std::vector<std::string>> enum_values;

    Json to_json_schema() const {
        Json schema{
            {"type", std::string(param_type_to_string(type))},
            {"description", description}
        };

        if (default_value) {
            schema["default"] = *default_value;
        }

        if (enum_values && !enum_values->empty()) {
            schema["enum"] = *enum_values;
        }

        return schema;
    }
};

// Tool specification (compatible with Claude's tool format)
struct ToolSpec {
    std::string name;
    std::string description;
    std::vector<ParamSpec> parameters;
    std::vector<std::string> keywords;  // For search/matching
    bool requires_confirmation = false;
    int timeout_ms = 60000;

    // Convert to Claude API format
    Json to_claude_format() const {
        Json properties = Json::object();
        std::vector<std::string> required_params;

        for (const auto& param : parameters) {
            properties[param.name] = param.to_json_schema();
            if (param.required) {
                required_params.push_back(param.name);
            }
        }

        return Json{
            {"name", name},
            {"description", description},
            {"input_schema", {
                {"type", "object"},
                {"properties", properties},
                {"required", required_params}
            }}
        };
    }

    // Convert to Gemini API format
    Json to_gemini_format() const {
        Json properties = Json::object();
        std::vector<std::string> required_params;

        for (const auto& param : parameters) {
            properties[param.name] = param.to_json_schema();
            if (param.required) {
                required_params.push_back(param.name);
            }
        }

        return Json{
            {"name", name},
            {"description", description},
            {"parameters", {
                {"type", "object"},
                {"properties", properties},
                {"required", required_params}
            }}
        };
    }
};

// Tool execution context
struct ToolContext {
    std::string session_id;
    std::string working_directory;
    std::vector<std::string> allowed_paths;
    bool sandbox_enabled = true;
    int max_output_lines = 2000;
    int timeout_ms = 60000;

    // Environment variables to pass to tools
    std::map<std::string, std::string> env;

    // Pointer to application config (for accessing API keys, search settings, etc.)
    const gpagent::core::Config* config = nullptr;
};

// Tool handler function type
using ToolHandler = std::function<ToolResult(const Json& args, const ToolContext& ctx)>;

}  // namespace gpagent::tools
