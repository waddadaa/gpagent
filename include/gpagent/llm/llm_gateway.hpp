#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "gpagent/core/types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gpagent::llm {

using namespace gpagent::core;

// Streaming callback (chunk-only version)
using StreamCallback = std::function<void(const std::string& chunk)>;

// Streaming callback with final flag
using StreamCallbackWithFinal = std::function<void(const std::string& chunk, bool is_final)>;

// LLM request
struct LLMRequest {
    std::vector<Message> messages;
    std::string system_prompt;
    Json tools;  // Tools in provider-specific format
    int max_tokens = 8192;
    float temperature = 0.7f;
    std::vector<std::string> stop_sequences;

    // Streaming callback (optional - if set, enables streaming)
    StreamCallback stream_callback;

    // Provider-specific options
    Json provider_options;
};

// Base LLM provider interface
class LLMProvider {
public:
    virtual ~LLMProvider() = default;

    // Get provider name
    virtual std::string name() const = 0;

    // Check if provider is available (API key set, etc.)
    virtual bool is_available() const = 0;

    // Send a request and get response
    virtual Result<LLMResponse, Error> complete(const LLMRequest& request) = 0;

    // Send a streaming request
    virtual Result<LLMResponse, Error> stream(const LLMRequest& request,
                                               StreamCallbackWithFinal callback) = 0;

    // Convert messages to provider-specific format
    virtual Json format_messages(const std::vector<Message>& messages) const = 0;

    // Convert tools to provider-specific format
    virtual Json format_tools(const Json& tools) const = 0;
};

// LLM Gateway - manages multiple providers with fallback
class LLMGateway {
public:
    explicit LLMGateway(const LLMConfig& config);
    LLMGateway(const LLMConfig& config, const ApiKeysConfig& api_keys);

    // Initialize the gateway (must be called before use)
    Result<void, Error> initialize();

    // Get primary provider
    LLMProvider& primary();
    const LLMProvider& primary() const;

    // Get fallback provider (if available)
    LLMProvider* fallback();

    // Get summarization provider
    LLMProvider* summarizer();

    // Complete request with automatic fallback
    Result<LLMResponse, Error> complete(const LLMRequest& request);

    // Stream request with automatic fallback
    Result<LLMResponse, Error> stream(const LLMRequest& request, StreamCallbackWithFinal callback);

    // Check if any provider is available
    bool is_available() const;

    // Get token usage statistics
    struct UsageStats {
        int64_t total_input_tokens = 0;
        int64_t total_output_tokens = 0;
        int requests = 0;
        int failures = 0;
        Duration total_latency{0};
    };
    UsageStats get_stats() const;
    void reset_stats();

private:
    LLMConfig config_;
    std::unique_ptr<LLMProvider> primary_provider_;
    std::unique_ptr<LLMProvider> fallback_provider_;
    std::unique_ptr<LLMProvider> summarizer_provider_;

    mutable UsageStats stats_;

    void record_request(const LLMResponse& response);
    void record_failure();

    std::unique_ptr<LLMProvider> create_provider(const std::string& name,
                                                   const std::string& model,
                                                   const ApiKeysConfig& api_keys);
};

}  // namespace gpagent::llm
