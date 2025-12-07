#include "gpagent/llm/llm_gateway.hpp"
#include "gpagent/llm/providers/claude.hpp"
#include "gpagent/llm/providers/gemini.hpp"

#include <cstdlib>

namespace gpagent::llm {

LLMGateway::LLMGateway(const LLMConfig& config)
    : config_(config)
{
}

LLMGateway::LLMGateway(const LLMConfig& config, const ApiKeysConfig& api_keys)
    : config_(config)
{
    primary_provider_ = create_provider(config.primary_provider, config.primary_model, api_keys);

    if (!config.fallback_provider.empty()) {
        fallback_provider_ = create_provider(config.fallback_provider, config.fallback_model, api_keys);
    }

    if (!config.summarization_model.empty()) {
        // Use same provider as primary for summarization, just different model
        summarizer_provider_ = create_provider(config.primary_provider, config.summarization_model, api_keys);
    }
}

Result<void, Error> LLMGateway::initialize() {
    // If providers were already created (via 2-arg constructor), just validate
    if (primary_provider_) {
        if (!primary_provider_->is_available()) {
            return Result<void, Error>::err(
                ErrorCode::LLMApiKeyMissing,
                "Primary LLM provider API key not set"
            );
        }
        return Result<void, Error>::ok();
    }

    // Get API keys from environment if not provided
    ApiKeysConfig api_keys;

    const char* anthropic_key = std::getenv("ANTHROPIC_API_KEY");
    if (anthropic_key) api_keys.anthropic = anthropic_key;

    const char* google_key = std::getenv("GOOGLE_API_KEY");
    if (google_key) api_keys.google = google_key;

    const char* openai_key = std::getenv("OPENAI_API_KEY");
    if (openai_key) api_keys.openai = openai_key;

    // Create providers
    primary_provider_ = create_provider(config_.primary_provider, config_.primary_model, api_keys);

    if (!primary_provider_) {
        return Result<void, Error>::err(
            ErrorCode::LLMProviderUnavailable,
            "Failed to create primary LLM provider: " + config_.primary_provider
        );
    }

    if (!config_.fallback_provider.empty()) {
        fallback_provider_ = create_provider(config_.fallback_provider, config_.fallback_model, api_keys);
    }

    if (!config_.summarization_model.empty()) {
        summarizer_provider_ = create_provider(config_.primary_provider, config_.summarization_model, api_keys);
    }

    return Result<void, Error>::ok();
}

std::unique_ptr<LLMProvider> LLMGateway::create_provider(const std::string& name,
                                                          const std::string& model,
                                                          const ApiKeysConfig& api_keys) {
    if (name == "claude" || name == "anthropic") {
        return std::make_unique<ClaudeProvider>(api_keys.anthropic, model);
    } else if (name == "gemini" || name == "google") {
        return std::make_unique<GeminiProvider>(api_keys.google, model);
    }

    return nullptr;
}

LLMProvider& LLMGateway::primary() {
    if (!primary_provider_) {
        throw std::runtime_error("No primary LLM provider configured");
    }
    return *primary_provider_;
}

const LLMProvider& LLMGateway::primary() const {
    if (!primary_provider_) {
        throw std::runtime_error("No primary LLM provider configured");
    }
    return *primary_provider_;
}

LLMProvider* LLMGateway::fallback() {
    return fallback_provider_.get();
}

LLMProvider* LLMGateway::summarizer() {
    return summarizer_provider_ ? summarizer_provider_.get() : primary_provider_.get();
}

bool LLMGateway::is_available() const {
    if (primary_provider_ && primary_provider_->is_available()) {
        return true;
    }
    if (fallback_provider_ && fallback_provider_->is_available()) {
        return true;
    }
    return false;
}

Result<LLMResponse, Error> LLMGateway::complete(const LLMRequest& request) {
    if (!primary_provider_) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMProviderUnavailable,
            "No LLM provider configured"
        );
    }

    // Try primary provider
    if (primary_provider_->is_available()) {
        auto result = primary_provider_->complete(request);
        if (result.is_ok()) {
            record_request(result.value());
            return result;
        }

        // If error is retriable and we have fallback, try fallback
        if (result.error().is_retriable() && fallback_provider_ && fallback_provider_->is_available()) {
            auto fallback_result = fallback_provider_->complete(request);
            if (fallback_result.is_ok()) {
                record_request(fallback_result.value());
                return fallback_result;
            }
            record_failure();
            return fallback_result;
        }

        record_failure();
        return result;
    }

    // Primary not available, try fallback
    if (fallback_provider_ && fallback_provider_->is_available()) {
        auto result = fallback_provider_->complete(request);
        if (result.is_ok()) {
            record_request(result.value());
        } else {
            record_failure();
        }
        return result;
    }

    return Result<LLMResponse, Error>::err(
        ErrorCode::LLMProviderUnavailable,
        "No LLM provider available"
    );
}

Result<LLMResponse, Error> LLMGateway::stream(const LLMRequest& request,
                                                StreamCallbackWithFinal callback) {
    if (!primary_provider_) {
        return Result<LLMResponse, Error>::err(
            ErrorCode::LLMProviderUnavailable,
            "No LLM provider configured"
        );
    }

    // Try primary provider
    if (primary_provider_->is_available()) {
        auto result = primary_provider_->stream(request, callback);
        if (result.is_ok()) {
            record_request(result.value());
            return result;
        }

        // If error is retriable and we have fallback, try fallback
        if (result.error().is_retriable() && fallback_provider_ && fallback_provider_->is_available()) {
            auto fallback_result = fallback_provider_->stream(request, callback);
            if (fallback_result.is_ok()) {
                record_request(fallback_result.value());
                return fallback_result;
            }
            record_failure();
            return fallback_result;
        }

        record_failure();
        return result;
    }

    // Primary not available, try fallback
    if (fallback_provider_ && fallback_provider_->is_available()) {
        auto result = fallback_provider_->stream(request, callback);
        if (result.is_ok()) {
            record_request(result.value());
        } else {
            record_failure();
        }
        return result;
    }

    return Result<LLMResponse, Error>::err(
        ErrorCode::LLMProviderUnavailable,
        "No LLM provider available"
    );
}

LLMGateway::UsageStats LLMGateway::get_stats() const {
    return stats_;
}

void LLMGateway::reset_stats() {
    stats_ = UsageStats{};
}

void LLMGateway::record_request(const LLMResponse& response) {
    stats_.total_input_tokens += response.usage.input_tokens;
    stats_.total_output_tokens += response.usage.output_tokens;
    stats_.total_latency += response.latency;
    stats_.requests++;
}

void LLMGateway::record_failure() {
    stats_.failures++;
}

}  // namespace gpagent::llm
