#pragma once

#include "gpagent/llm/llm_gateway.hpp"

#include <string>

namespace gpagent::llm {

class GeminiProvider : public LLMProvider {
public:
    GeminiProvider(const std::string& api_key, const std::string& model);

    std::string name() const override { return "gemini"; }
    bool is_available() const override;

    Result<LLMResponse, Error> complete(const LLMRequest& request) override;
    Result<LLMResponse, Error> stream(const LLMRequest& request,
                                        StreamCallbackWithFinal callback) override;

    Json format_messages(const std::vector<Message>& messages) const override;
    Json format_tools(const Json& tools) const override;

private:
    std::string api_key_;
    std::string model_;
    std::string base_url_ = "https://generativelanguage.googleapis.com/v1beta";

    // Parse Gemini API response
    Result<LLMResponse, Error> parse_response(const std::string& body);
};

}  // namespace gpagent::llm
