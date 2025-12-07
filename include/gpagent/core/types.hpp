#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace gpagent::core {

// JSON alias
using Json = nlohmann::json;

// Time types
using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;
using Duration = std::chrono::milliseconds;

// Common type aliases
using SessionId = std::string;
using ThreadId = std::string;
using EpisodeId = std::string;
using CheckpointId = std::string;
using ToolId = std::string;

// Message roles
enum class Role {
    System,
    User,
    Assistant,
    Tool
};

inline std::string_view role_to_string(Role role) {
    switch (role) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
        case Role::Tool: return "tool";
    }
    return "unknown";
}

inline Role role_from_string(std::string_view str) {
    if (str == "system") return Role::System;
    if (str == "user") return Role::User;
    if (str == "assistant") return Role::Assistant;
    if (str == "tool") return Role::Tool;
    return Role::User;
}

// Tool call structure
struct ToolCall {
    std::string id;
    ToolId tool_name;
    Json arguments;

    Json to_json() const {
        return Json{
            {"id", id},
            {"name", tool_name},
            {"arguments", arguments}
        };
    }

    static ToolCall from_json(const Json& j) {
        return ToolCall{
            .id = j.value("id", ""),
            .tool_name = j.value("name", ""),
            .arguments = j.value("arguments", Json::object())
        };
    }
};

// Tool result structure
struct ToolResult {
    std::string tool_call_id;
    bool success;
    std::string content;
    std::optional<std::string> error_message;
    Duration execution_time{0};
    bool is_image = false;  // Flag for image content (base64 encoded)

    Json to_json() const {
        Json j{
            {"tool_call_id", tool_call_id},
            {"success", success},
            {"content", content},
            {"execution_time_ms", execution_time.count()}
        };
        if (error_message) {
            j["error"] = *error_message;
        }
        return j;
    }
};

// Image content for multimodal messages
struct ImageContent {
    std::string data;       // Base64 encoded image data
    std::string media_type; // e.g., "image/jpeg", "image/png"
    std::string source_path; // Original file path (for reference)

    Json to_json() const {
        return Json{
            {"type", "image"},
            {"source", {
                {"type", "base64"},
                {"media_type", media_type},
                {"data", data}
            }}
        };
    }
};

// Message structure
struct Message {
    Role role;
    std::string content;
    std::optional<std::string> name;  // For tool messages
    std::vector<ToolCall> tool_calls;
    std::optional<std::string> tool_call_id;  // For tool results
    std::vector<ImageContent> images;  // Attached images for multimodal
    TimePoint timestamp;

    Message() : role(Role::User), timestamp(Clock::now()) {}

    Message(Role r, std::string c)
        : role(r), content(std::move(c)), timestamp(Clock::now()) {}

    static Message user(std::string content) {
        return Message{Role::User, std::move(content)};
    }

    static Message assistant(std::string content) {
        return Message{Role::Assistant, std::move(content)};
    }

    static Message system(std::string content) {
        return Message{Role::System, std::move(content)};
    }

    static Message tool_result(std::string tool_call_id, std::string content) {
        Message m{Role::Tool, std::move(content)};
        m.tool_call_id = std::move(tool_call_id);
        return m;
    }

    Json to_json() const {
        Json j{
            {"role", std::string(role_to_string(role))},
            {"content", content},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                timestamp.time_since_epoch()).count()}
        };
        if (name) j["name"] = *name;
        if (!tool_calls.empty()) {
            j["tool_calls"] = Json::array();
            for (const auto& tc : tool_calls) {
                j["tool_calls"].push_back(tc.to_json());
            }
        }
        if (tool_call_id) j["tool_call_id"] = *tool_call_id;
        return j;
    }

    static Message from_json(const Json& j) {
        Message m;
        m.role = role_from_string(j.value("role", "user"));
        m.content = j.value("content", "");
        if (j.contains("name")) m.name = j["name"].get<std::string>();
        if (j.contains("tool_call_id")) m.tool_call_id = j["tool_call_id"].get<std::string>();
        if (j.contains("tool_calls")) {
            for (const auto& tc : j["tool_calls"]) {
                m.tool_calls.push_back(ToolCall::from_json(tc));
            }
        }
        if (j.contains("timestamp")) {
            m.timestamp = TimePoint{std::chrono::seconds{j["timestamp"].get<int64_t>()}};
        }
        return m;
    }
};

// Stop reason for LLM responses
enum class StopReason {
    EndTurn,
    MaxTokens,
    ToolUse,
    StopSequence,
    Error
};

inline std::string_view stop_reason_to_string(StopReason reason) {
    switch (reason) {
        case StopReason::EndTurn: return "end_turn";
        case StopReason::MaxTokens: return "max_tokens";
        case StopReason::ToolUse: return "tool_use";
        case StopReason::StopSequence: return "stop_sequence";
        case StopReason::Error: return "error";
    }
    return "unknown";
}

// Token usage
struct TokenUsage {
    int input_tokens = 0;
    int output_tokens = 0;

    int total() const { return input_tokens + output_tokens; }

    Json to_json() const {
        return Json{
            {"input_tokens", input_tokens},
            {"output_tokens", output_tokens},
            {"total_tokens", total()}
        };
    }
};

// LLM Response
struct LLMResponse {
    std::string content;
    std::vector<ToolCall> tool_calls;
    StopReason stop_reason = StopReason::EndTurn;
    TokenUsage usage;
    std::string model;
    Duration latency{0};

    bool has_tool_calls() const { return !tool_calls.empty(); }
};

}  // namespace gpagent::core
