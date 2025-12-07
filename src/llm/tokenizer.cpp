#include "gpagent/core/types.hpp"

namespace gpagent::llm {

using namespace gpagent::core;

// Simple token estimation (not exact, but good enough for planning)
// Based on approximately 4 characters per token for English text

class Tokenizer {
public:
    // Estimate token count for text
    static int estimate_tokens(const std::string& text) {
        // Rough estimate: ~4 characters per token
        // JSON/code tends to be slightly more tokens per character
        return static_cast<int>(text.length() / 3.5);
    }

    // Estimate tokens for a message
    static int estimate_tokens(const Message& message) {
        int tokens = 0;

        // Role takes a few tokens
        tokens += 3;

        // Content
        tokens += estimate_tokens(message.content);

        // Tool calls
        for (const auto& tc : message.tool_calls) {
            tokens += 10;  // Tool call overhead
            tokens += estimate_tokens(tc.tool_name);
            tokens += estimate_tokens(tc.arguments.dump());
        }

        return tokens;
    }

    // Estimate tokens for multiple messages
    static int estimate_tokens(const std::vector<Message>& messages) {
        int tokens = 0;
        for (const auto& msg : messages) {
            tokens += estimate_tokens(msg);
        }
        return tokens;
    }

    // Estimate tokens for JSON
    static int estimate_tokens(const Json& j) {
        return estimate_tokens(j.dump());
    }
};

}  // namespace gpagent::llm
