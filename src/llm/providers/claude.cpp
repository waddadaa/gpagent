#include "gpagent/llm/providers/claude.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <set>
#include <spdlog/spdlog.h>

namespace gpagent::llm {

ClaudeProvider::ClaudeProvider(const std::string& api_key, const std::string& model)
    : api_key_(api_key)
    , model_(model)
{
}

bool ClaudeProvider::is_available() const {
    return !api_key_.empty();
}

Json ClaudeProvider::format_messages(const std::vector<Message>& messages) const {
    Json formatted = Json::array();

    spdlog::info("format_messages: processing {} messages", messages.size());

    // First pass: collect all tool_use IDs from assistant messages
    std::set<std::string> valid_tool_ids;
    for (const auto& msg : messages) {
        spdlog::info("  Message role={}, content_len={}, tool_calls={}, tool_call_id={}",
            static_cast<int>(msg.role),
            msg.content.size(),
            msg.tool_calls.size(),
            msg.tool_call_id.value_or("none"));
        if (msg.role == Role::Assistant) {
            for (const auto& tc : msg.tool_calls) {
                spdlog::info("    Found tool_use id={} name={}", tc.id, tc.tool_name);
                valid_tool_ids.insert(tc.id);
            }
        }
    }

    spdlog::info("Valid tool IDs collected: {}", valid_tool_ids.size());

    for (const auto& msg : messages) {
        if (msg.role == Role::System) {
            continue;  // System messages handled separately
        }

        // Skip orphan tool results (tool_result without corresponding tool_use)
        if (msg.role == Role::Tool) {
            auto tool_id = msg.tool_call_id.value_or("");
            if (valid_tool_ids.find(tool_id) == valid_tool_ids.end()) {
                spdlog::warn("Skipping orphan tool_result with id={}", tool_id);
                continue;  // Skip this orphan tool result
            }
        }

        Json content;
        if (msg.role == Role::Tool) {
            // Tool result - may include images
            content = Json::array();

            // Build tool_result content (can be array with images or just text)
            Json tool_content;
            if (!msg.images.empty()) {
                // Tool result with images - use array format
                tool_content = Json::array();

                // Add images first
                for (const auto& img : msg.images) {
                    tool_content.push_back(Json{
                        {"type", "image"},
                        {"source", {
                            {"type", "base64"},
                            {"media_type", img.media_type},
                            {"data", img.data}
                        }}
                    });
                }

                // Add text description
                if (!msg.content.empty()) {
                    tool_content.push_back(Json{
                        {"type", "text"},
                        {"text", msg.content}
                    });
                }
            } else {
                // Simple text content
                tool_content = msg.content;
            }

            content.push_back(Json{
                {"type", "tool_result"},
                {"tool_use_id", msg.tool_call_id.value_or("")},
                {"content", tool_content}
            });
        } else if (!msg.tool_calls.empty()) {
            // Assistant message with tool calls
            content = Json::array();

            if (!msg.content.empty()) {
                content.push_back(Json{
                    {"type", "text"},
                    {"text", msg.content}
                });
            }

            for (const auto& tc : msg.tool_calls) {
                content.push_back(Json{
                    {"type", "tool_use"},
                    {"id", tc.id},
                    {"name", tc.tool_name},
                    {"input", tc.arguments}
                });
            }
        } else if (!msg.images.empty()) {
            // Message with images (multimodal)
            content = Json::array();

            // Add images first
            for (const auto& img : msg.images) {
                content.push_back(Json{
                    {"type", "image"},
                    {"source", {
                        {"type", "base64"},
                        {"media_type", img.media_type},
                        {"data", img.data}
                    }}
                });
            }

            // Then add text content
            if (!msg.content.empty()) {
                content.push_back(Json{
                    {"type", "text"},
                    {"text", msg.content}
                });
            }
        } else {
            // Regular text message
            content = msg.content;
        }

        // Determine role - tool results must be sent as "user" role per Claude API
        std::string role_str;
        if (msg.role == Role::User || msg.role == Role::Tool) {
            role_str = "user";
        } else {
            role_str = "assistant";
        }

        formatted.push_back(Json{
            {"role", role_str},
            {"content", content}
        });
    }

    return formatted;
}

Json ClaudeProvider::format_tools(const Json& tools) const {
    // Claude format is already compatible
    return tools;
}

Result<LLMResponse, Error> ClaudeProvider::parse_response(const std::string& body) {
    try {
        Json j = Json::parse(body);

        // Check for error
        if (j.contains("error")) {
            std::string error_type = j["error"].value("type", "unknown");
            std::string error_msg = j["error"].value("message", "Unknown error");

            if (error_type == "rate_limit_error") {
                return Result<LLMResponse, Error>::err(ErrorCode::LLMRateLimited, error_msg);
            } else if (error_type == "overloaded_error") {
                return Result<LLMResponse, Error>::err(ErrorCode::LLMProviderUnavailable, error_msg);
            } else if (error_type == "invalid_request_error") {
                return Result<LLMResponse, Error>::err(ErrorCode::InvalidArgument, error_msg);
            }

            return Result<LLMResponse, Error>::err(ErrorCode::LLMInvalidResponse, error_msg);
        }

        LLMResponse response;
        response.model = j.value("model", model_);

        // Parse content
        if (j.contains("content")) {
            for (const auto& block : j["content"]) {
                if (block["type"] == "text") {
                    response.content += block.value("text", "");
                } else if (block["type"] == "tool_use") {
                    ToolCall tc;
                    tc.id = block.value("id", "");
                    tc.tool_name = block.value("name", "");
                    tc.arguments = block.value("input", Json::object());
                    response.tool_calls.push_back(std::move(tc));
                }
            }
        }

        // Parse stop reason
        std::string stop_reason = j.value("stop_reason", "end_turn");
        if (stop_reason == "end_turn") {
            response.stop_reason = StopReason::EndTurn;
        } else if (stop_reason == "max_tokens") {
            response.stop_reason = StopReason::MaxTokens;
        } else if (stop_reason == "tool_use") {
            response.stop_reason = StopReason::ToolUse;
        } else if (stop_reason == "stop_sequence") {
            response.stop_reason = StopReason::StopSequence;
        }

        // Parse usage
        if (j.contains("usage")) {
            response.usage.input_tokens = j["usage"].value("input_tokens", 0);
            response.usage.output_tokens = j["usage"].value("output_tokens", 0);
        }

        return Result<LLMResponse, Error>::ok(std::move(response));

    } catch (const Json::exception& e) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMInvalidResponse,
            std::string("JSON parse error: ") + e.what()
        );
    }
}

Result<LLMResponse, Error> ClaudeProvider::complete(const LLMRequest& request) {
    if (!is_available()) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMApiKeyMissing,
            "Anthropic API key not set"
        );
    }

    auto start = std::chrono::steady_clock::now();

    httplib::Client client(base_url_);
    client.set_read_timeout(120);
    client.set_connection_timeout(30);

    // Build request body
    Json body;
    body["model"] = model_;
    body["max_tokens"] = request.max_tokens;
    body["messages"] = format_messages(request.messages);

    if (!request.system_prompt.empty()) {
        body["system"] = request.system_prompt;
    }

    if (!request.tools.empty()) {
        body["tools"] = format_tools(request.tools);
    }

    if (request.temperature > 0) {
        body["temperature"] = request.temperature;
    }

    if (!request.stop_sequences.empty()) {
        body["stop_sequences"] = request.stop_sequences;
    }

    // Add headers
    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"X-API-Key", api_key_},
        {"anthropic-version", api_version_}
    };

    auto res = client.Post("/v1/messages", headers, body.dump(), "application/json");

    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<Duration>(end - start);

    if (!res) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMConnectionFailed,
            "Failed to connect to Anthropic API"
        );
    }

    if (res->status == 429) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMRateLimited,
            "Rate limited by Anthropic API"
        );
    }

    if (res->status != 200) {
        auto result = parse_response(res->body);
        if (result.is_err()) {
            return result;
        }
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMInvalidResponse,
            "Unexpected status code: " + std::to_string(res->status)
        );
    }

    auto result = parse_response(res->body);
    if (result.is_ok()) {
        result.value().latency = latency;

        // If a stream callback was provided, call it with the complete response
        if (request.stream_callback && !result.value().content.empty()) {
            request.stream_callback(result.value().content);
        }
    }

    return result;
}

void ClaudeProvider::parse_sse_event(const std::string& event, LLMResponse& response,
                                       StreamCallbackWithFinal& callback) {
    try {
        Json j = Json::parse(event);
        std::string type = j.value("type", "");

        if (type == "content_block_delta") {
            if (j.contains("delta")) {
                const auto& delta = j["delta"];
                if (delta.contains("text")) {
                    std::string text = delta["text"].get<std::string>();
                    response.content += text;
                    callback(text, false);
                }
            }
        } else if (type == "message_delta") {
            if (j.contains("delta")) {
                const auto& delta = j["delta"];
                if (delta.contains("stop_reason")) {
                    std::string reason = delta["stop_reason"].get<std::string>();
                    if (reason == "end_turn") {
                        response.stop_reason = StopReason::EndTurn;
                    } else if (reason == "tool_use") {
                        response.stop_reason = StopReason::ToolUse;
                    } else if (reason == "max_tokens") {
                        response.stop_reason = StopReason::MaxTokens;
                    }
                }
            }
            if (j.contains("usage")) {
                response.usage.output_tokens = j["usage"].value("output_tokens", 0);
            }
        } else if (type == "message_start") {
            if (j.contains("message")) {
                response.model = j["message"].value("model", model_);
                if (j["message"].contains("usage")) {
                    response.usage.input_tokens = j["message"]["usage"].value("input_tokens", 0);
                }
            }
        } else if (type == "content_block_start") {
            if (j.contains("content_block")) {
                const auto& block = j["content_block"];
                if (block.value("type", "") == "tool_use") {
                    ToolCall tc;
                    tc.id = block.value("id", "");
                    tc.tool_name = block.value("name", "");
                    tc.arguments = Json::object();
                    response.tool_calls.push_back(std::move(tc));
                }
            }
        }
    } catch (...) {
        // Ignore parse errors in streaming
    }
}

Result<LLMResponse, Error> ClaudeProvider::stream(const LLMRequest& request,
                                                    StreamCallbackWithFinal callback) {
    // For now, use the complete() method and simulate streaming
    // TODO: Implement proper SSE streaming when httplib API is stabilized
    auto result = complete(request);
    if (result.is_err()) {
        return result;
    }

    auto& response = result.value();

    // Simulate streaming by sending the content in chunks
    const size_t chunk_size = 50;
    for (size_t i = 0; i < response.content.size(); i += chunk_size) {
        std::string chunk = response.content.substr(i, chunk_size);
        bool is_final = (i + chunk_size >= response.content.size());
        callback(chunk, is_final);
    }

    if (response.content.empty()) {
        callback("", true);
    }

    return result;

    /* TODO: Re-enable proper streaming when httplib interface is resolved
    auto start = std::chrono::steady_clock::now();

    httplib::Client client(base_url_);
    client.set_read_timeout(120);
    client.set_connection_timeout(30);

    // Build request body
    Json body;
    body["model"] = model_;
    body["max_tokens"] = request.max_tokens;
    body["messages"] = format_messages(request.messages);
    body["stream"] = true;

    if (!request.system_prompt.empty()) {
        body["system"] = request.system_prompt;
    }

    if (!request.tools.empty()) {
        body["tools"] = format_tools(request.tools);
    }

    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"X-API-Key", api_key_},
        {"anthropic-version", api_version_}
    };

    LLMResponse response;
    response.model = model_;
    std::string buffer;

    auto res = client.Post("/v1/messages", headers, body.dump(), "application/json",
        [&](const char* data, size_t len) -> bool {
            buffer.append(data, len);

            // Parse SSE events
            size_t pos;
            while ((pos = buffer.find("\n\n")) != std::string::npos) {
                std::string event_block = buffer.substr(0, pos);
                buffer = buffer.substr(pos + 2);

                // Extract data from event
                size_t data_pos = event_block.find("data: ");
                if (data_pos != std::string::npos) {
                    std::string event_data = event_block.substr(data_pos + 6);
                    if (event_data != "[DONE]") {
                        parse_sse_event(event_data, response, callback);
                    }
                }
            }

            return true;
        });

    auto end = std::chrono::steady_clock::now();
    response.latency = std::chrono::duration_cast<Duration>(end - start);

    if (!res) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMConnectionFailed,
            "Failed to connect to Anthropic API"
        );
    }

    // Signal completion
    callback("", true);

    return Result<LLMResponse, Error>::ok(std::move(response));
    */
}

}  // namespace gpagent::llm
