#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "gpagent/core/types.hpp"
#include "gpagent/memory/memory_manager.hpp"
#include "gpagent/llm/llm_gateway.hpp"

#include <string>
#include <vector>

namespace gpagent::context {

using namespace gpagent::core;

// Context window structure
struct ContextWindow {
    std::string system_prompt;
    std::vector<Message> messages;
    Json tools;

    int estimated_tokens = 0;
    bool was_compacted = false;
};

// Context builder - constructs the context window for LLM requests
class ContextBuilder {
public:
    ContextBuilder(const ContextConfig& config);

    // Set the base system prompt
    ContextBuilder& with_system_prompt(const std::string& prompt);

    // Add user memory context
    ContextBuilder& with_user_memory(const std::string& memory);

    // Add project memory context
    ContextBuilder& with_project_memory(const std::string& memory);

    // Add compressed history
    ContextBuilder& with_compressed_history(const std::string& history);

    // Add recent messages
    ContextBuilder& with_messages(const std::vector<Message>& messages);

    // Add tools
    ContextBuilder& with_tools(const Json& tools);

    // Add relevant episodes
    ContextBuilder& with_episodes(const std::vector<memory::Episode>& episodes);

    // Add current task context
    ContextBuilder& with_task_context(const std::string& task);

    // Build the final context window
    Result<ContextWindow, Error> build();

    // Get estimated token count
    int estimated_tokens() const;

private:
    ContextConfig config_;

    std::string system_prompt_;
    std::string user_memory_;
    std::string project_memory_;
    std::string compressed_history_;
    std::vector<Message> messages_;
    Json tools_;
    std::string episodes_context_;
    std::string task_context_;

    // Token estimation
    int estimate_tokens(const std::string& text) const;
    int estimate_message_tokens(const Message& msg) const;
};

// Context compactor - compresses context when it gets too large
class ContextCompactor {
public:
    ContextCompactor(const ContextConfig& config, llm::LLMGateway& llm);

    // Check if compaction is needed
    bool needs_compaction(int current_tokens) const;

    // Compact messages into summaries
    Result<std::string, Error> compact_messages(const std::vector<Message>& messages,
                                                  int start_idx, int end_idx);

    // Get the system prompt for summarization
    static std::string summarization_prompt();

private:
    ContextConfig config_;
    llm::LLMGateway& llm_;
};

// Context manager - high-level interface for context operations
class ContextManager {
public:
    ContextManager(const ContextConfig& config, llm::LLMGateway& llm);

    // Build context for a request
    Result<ContextWindow, Error> build_context(
        memory::MemoryManager& memory,
        const std::string& system_prompt,
        const Json& tools,
        const std::string& current_task = ""
    );

    // Compact context if needed
    Result<void, Error> compact_if_needed(memory::MemoryManager& memory);

    // Get token budget remaining
    int remaining_tokens(int current_tokens) const;

    // Check if we're approaching token limit
    bool is_near_limit(int current_tokens) const;

private:
    ContextConfig config_;
    llm::LLMGateway& llm_;
    std::unique_ptr<ContextCompactor> compactor_;
};

}  // namespace gpagent::context
