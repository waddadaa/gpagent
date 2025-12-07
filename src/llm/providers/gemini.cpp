#include "gpagent/llm/providers/gemini.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

namespace gpagent::llm {

GeminiProvider::GeminiProvider(const std::string& api_key, const std::string& model)
    : api_key_(api_key)
    , model_(model)
{
}

bool GeminiProvider::is_available() const {
    return !api_key_.empty();
}

Json GeminiProvider::format_messages(const std::vector<Message>& messages) const {
    Json contents = Json::array();

    for (const auto& msg : messages) {
        if (msg.role == Role::System) {
            continue;  // System instructions handled separately
        }

        Json parts = Json::array();

        if (msg.role == Role::Tool) {
            // Function response
            parts.push_back(Json{
                {"functionResponse", {
                    {"name", msg.name.value_or("")},
                    {"response", {{"content", msg.content}}}
                }}
            });
        } else if (!msg.tool_calls.empty()) {
            // Function calls
            if (!msg.content.empty()) {
                parts.push_back(Json{{"text", msg.content}});
            }
            for (const auto& tc : msg.tool_calls) {
                parts.push_back(Json{
                    {"functionCall", {
                        {"name", tc.tool_name},
                        {"args", tc.arguments}
                    }}
                });
            }
        } else if (!msg.images.empty()) {
            // Message with images (multimodal)
            // Add images first
            for (const auto& img : msg.images) {
                parts.push_back(Json{
                    {"inline_data", {
                        {"mime_type", img.media_type},
                        {"data", img.data}
                    }}
                });
            }
            // Then add text content
            if (!msg.content.empty()) {
                parts.push_back(Json{{"text", msg.content}});
            }
        } else {
            parts.push_back(Json{{"text", msg.content}});
        }

        std::string role = (msg.role == Role::User) ? "user" : "model";
        contents.push_back(Json{
            {"role", role},
            {"parts", parts}
        });
    }

    return contents;
}

Json GeminiProvider::format_tools(const Json& tools) const {
    // Convert from Claude format to Gemini format
    Json function_declarations = Json::array();

    for (const auto& tool : tools) {
        Json decl;
        decl["name"] = tool.value("name", "");
        decl["description"] = tool.value("description", "");

        if (tool.contains("input_schema")) {
            decl["parameters"] = tool["input_schema"];
        } else if (tool.contains("parameters")) {
            decl["parameters"] = tool["parameters"];
        }

        function_declarations.push_back(decl);
    }

    return Json{{"functionDeclarations", function_declarations}};
}

Result<LLMResponse, Error> GeminiProvider::parse_response(const std::string& body) {
    try {
        Json j = Json::parse(body);

        // Check for error
        if (j.contains("error")) {
            std::string error_msg = j["error"].value("message", "Unknown error");
            int code = j["error"].value("code", 0);

            if (code == 429) {
                return Result<LLMResponse, Error>::err(ErrorCode::LLMRateLimited, error_msg);
            } else if (code == 503) {
                return Result<LLMResponse, Error>::err(ErrorCode::LLMProviderUnavailable, error_msg);
            }

            return Result<LLMResponse, Error>::err(ErrorCode::LLMInvalidResponse, error_msg);
        }

        LLMResponse response;
        response.model = model_;

        // Parse candidates
        if (j.contains("candidates") && !j["candidates"].empty()) {
            const auto& candidate = j["candidates"][0];

            if (candidate.contains("content") && candidate["content"].contains("parts")) {
                for (const auto& part : candidate["content"]["parts"]) {
                    if (part.contains("text")) {
                        response.content += part["text"].get<std::string>();
                    } else if (part.contains("functionCall")) {
                        ToolCall tc;
                        tc.id = "fc_" + std::to_string(response.tool_calls.size());
                        tc.tool_name = part["functionCall"].value("name", "");
                        tc.arguments = part["functionCall"].value("args", Json::object());
                        response.tool_calls.push_back(std::move(tc));
                    }
                }
            }

            // Parse finish reason
            std::string finish_reason = candidate.value("finishReason", "STOP");
            if (finish_reason == "STOP") {
                response.stop_reason = StopReason::EndTurn;
            } else if (finish_reason == "MAX_TOKENS") {
                response.stop_reason = StopReason::MaxTokens;
            } else if (finish_reason == "TOOL_USE" || !response.tool_calls.empty()) {
                response.stop_reason = StopReason::ToolUse;
            }
        }

        // Parse usage
        if (j.contains("usageMetadata")) {
            response.usage.input_tokens = j["usageMetadata"].value("promptTokenCount", 0);
            response.usage.output_tokens = j["usageMetadata"].value("candidatesTokenCount", 0);
        }

        return Result<LLMResponse, Error>::ok(std::move(response));

    } catch (const Json::exception& e) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMInvalidResponse,
            std::string("JSON parse error: ") + e.what()
        );
    }
}

Result<LLMResponse, Error> GeminiProvider::complete(const LLMRequest& request) {
    if (!is_available()) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMApiKeyMissing,
            "Google API key not set"
        );
    }

    auto start = std::chrono::steady_clock::now();

    httplib::Client client("https://generativelanguage.googleapis.com");
    client.set_read_timeout(120);
    client.set_connection_timeout(30);

    // Build request body
    Json body;
    body["contents"] = format_messages(request.messages);

    if (!request.system_prompt.empty()) {
        body["systemInstruction"] = Json{
            {"parts", Json::array({{{"text", request.system_prompt}}})}
        };
    }

    if (!request.tools.empty()) {
        body["tools"] = Json::array({format_tools(request.tools)});
    }

    // Generation config
    Json gen_config;
    gen_config["maxOutputTokens"] = request.max_tokens;
    if (request.temperature > 0) {
        gen_config["temperature"] = request.temperature;
    }
    if (!request.stop_sequences.empty()) {
        gen_config["stopSequences"] = request.stop_sequences;
    }
    body["generationConfig"] = gen_config;

    // Build URL with API key
    std::string url = "/v1beta/models/" + model_ + ":generateContent?key=" + api_key_;

    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };

    auto res = client.Post(url, headers, body.dump(), "application/json");

    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<Duration>(end - start);

    if (!res) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMConnectionFailed,
            "Failed to connect to Gemini API"
        );
    }

    if (res->status == 429) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMRateLimited,
            "Rate limited by Gemini API"
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
    }

    return result;
}

Result<LLMResponse, Error> GeminiProvider::stream(const LLMRequest& request,
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
}

}  // namespace gpagent::llm
