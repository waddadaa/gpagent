#pragma once

#include "gpagent/llm/llm_gateway.hpp"

#include <string>

namespace gpagent::llm {

class ClaudeProvider : public LLMProvider {
public:
    ClaudeProvider(const std::string& api_key, const std::string& model);

    std::string name() const override { return "claude"; }
    bool is_available() const override;

    Result<LLMResponse, Error> complete(const LLMRequest& request) override;
    Result<LLMResponse, Error> stream(const LLMRequest& request,
                                        StreamCallbackWithFinal callback) override;

    Json format_messages(const std::vector<Message>& messages) const override;
    Json format_tools(const Json& tools) const override;

private:
    std::string api_key_;
    std::string model_;
    std::string base_url_ = "https://api.anthropic.com";
    std::string api_version_ = "2023-06-01";

    // Parse Claude API response
    Result<LLMResponse, Error> parse_response(const std::string& body);

    // Parse streaming SSE events
    void parse_sse_event(const std::string& event, LLMResponse& response,
                          StreamCallbackWithFinal& callback);
};

}  // namespace gpagent::llm
