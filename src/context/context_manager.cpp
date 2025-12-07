#include "gpagent/context/context_manager.hpp"

#include <sstream>

namespace gpagent::context {

// ContextBuilder
ContextBuilder::ContextBuilder(const ContextConfig& config)
    : config_(config)
{
}

ContextBuilder& ContextBuilder::with_system_prompt(const std::string& prompt) {
    system_prompt_ = prompt;
    return *this;
}

ContextBuilder& ContextBuilder::with_user_memory(const std::string& memory) {
    user_memory_ = memory;
    return *this;
}

ContextBuilder& ContextBuilder::with_project_memory(const std::string& memory) {
    project_memory_ = memory;
    return *this;
}

ContextBuilder& ContextBuilder::with_compressed_history(const std::string& history) {
    compressed_history_ = history;
    return *this;
}

ContextBuilder& ContextBuilder::with_messages(const std::vector<Message>& messages) {
    messages_ = messages;
    return *this;
}

ContextBuilder& ContextBuilder::with_tools(const Json& tools) {
    tools_ = tools;
    return *this;
}

ContextBuilder& ContextBuilder::with_episodes(const std::vector<memory::Episode>& episodes) {
    if (episodes.empty()) return *this;

    std::ostringstream ss;
    ss << "## Relevant Past Experiences\n\n";

    for (size_t i = 0; i < std::min(episodes.size(), size_t(3)); ++i) {
        const auto& ep = episodes[i];
        ss << "### " << ep.task_description << "\n";
        ss << "- Outcome: " << (ep.outcome.success ? "Success" : "Failed") << "\n";
        ss << "- Tools used: ";
        for (size_t j = 0; j < std::min(ep.actions.size(), size_t(5)); ++j) {
            if (j > 0) ss << ", ";
            ss << ep.actions[j].tool;
        }
        ss << "\n";

        if (!ep.learnings.empty()) {
            ss << "- Learnings:\n";
            for (const auto& learning : ep.learnings) {
                ss << "  - " << learning << "\n";
            }
        }
        ss << "\n";
    }

    episodes_context_ = ss.str();
    return *this;
}

ContextBuilder& ContextBuilder::with_task_context(const std::string& task) {
    task_context_ = task;
    return *this;
}

int ContextBuilder::estimate_tokens(const std::string& text) const {
    // Rough estimate: ~3.5 characters per token
    return static_cast<int>(text.length() / 3.5);
}

int ContextBuilder::estimate_message_tokens(const Message& msg) const {
    int tokens = 3;  // Role overhead
    tokens += estimate_tokens(msg.content);

    for (const auto& tc : msg.tool_calls) {
        tokens += 10;
        tokens += estimate_tokens(tc.tool_name);
        tokens += estimate_tokens(tc.arguments.dump());
    }

    return tokens;
}

int ContextBuilder::estimated_tokens() const {
    int tokens = 0;

    tokens += estimate_tokens(system_prompt_);
    tokens += estimate_tokens(user_memory_);
    tokens += estimate_tokens(project_memory_);
    tokens += estimate_tokens(compressed_history_);
    tokens += estimate_tokens(episodes_context_);
    tokens += estimate_tokens(task_context_);

    for (const auto& msg : messages_) {
        tokens += estimate_message_tokens(msg);
    }

    if (!tools_.empty()) {
        tokens += estimate_tokens(tools_.dump());
    }

    return tokens;
}

Result<ContextWindow, Error> ContextBuilder::build() {
    ContextWindow window;

    // Build system prompt with all context
    std::ostringstream system;
    system << system_prompt_;

    if (!user_memory_.empty()) {
        system << "\n\n## User Memory\n" << user_memory_;
    }

    if (!project_memory_.empty()) {
        system << "\n\n## Project Memory\n" << project_memory_;
    }

    if (!compressed_history_.empty()) {
        system << "\n\n## Conversation History Summary\n" << compressed_history_;
    }

    if (!episodes_context_.empty()) {
        system << "\n\n" << episodes_context_;
    }

    if (!task_context_.empty()) {
        system << "\n\n## Current Task\n" << task_context_;
    }

    window.system_prompt = system.str();
    window.messages = messages_;
    window.tools = tools_;
    window.estimated_tokens = estimated_tokens();

    // Check if we exceeded context limit
    if (window.estimated_tokens > config_.max_tokens) {
        return Result<ContextWindow, Error>::err(
            ErrorCode::ContextTooLarge,
            "Context exceeds maximum tokens: " +
                std::to_string(window.estimated_tokens) + " > " +
                std::to_string(config_.max_tokens)
        );
    }

    return Result<ContextWindow, Error>::ok(std::move(window));
}

// ContextCompactor
ContextCompactor::ContextCompactor(const ContextConfig& config, llm::LLMGateway& llm)
    : config_(config)
    , llm_(llm)
{
}

bool ContextCompactor::needs_compaction(int current_tokens) const {
    return current_tokens > config_.compaction_threshold;
}

std::string ContextCompactor::summarization_prompt() {
    return R"(You are a conversation summarizer. Summarize the following conversation excerpt concisely, focusing on:
1. Key decisions made
2. Important information learned
3. Actions taken and their outcomes
4. Any pending items or context needed for future turns

Be concise but preserve all important details. Output only the summary, no preamble.)";
}

Result<std::string, Error> ContextCompactor::compact_messages(
    const std::vector<Message>& messages, int start_idx, int end_idx) {

    if (start_idx >= end_idx || start_idx < 0 ||
        end_idx > static_cast<int>(messages.size())) {
        return Result<std::string, Error>::err(
            ErrorCode::InvalidArgument,
            "Invalid message range for compaction"
        );
    }

    // Build conversation text to summarize
    std::ostringstream conv;
    for (int i = start_idx; i < end_idx; ++i) {
        const auto& msg = messages[i];
        conv << std::string(role_to_string(msg.role)) << ": ";
        conv << msg.content << "\n";

        for (const auto& tc : msg.tool_calls) {
            conv << "[Tool: " << tc.tool_name << "]\n";
        }
        conv << "\n";
    }

    // Request summarization from LLM
    llm::LLMRequest request;
    request.system_prompt = summarization_prompt();
    request.messages = {Message::user(conv.str())};
    request.max_tokens = 1000;
    request.temperature = 0.3;

    auto* summarizer = llm_.summarizer();
    if (!summarizer) {
        return Result<std::string, Error>::err(
            ErrorCode::LLMProviderUnavailable,
            "No summarization provider available"
        );
    }

    auto result = summarizer->complete(request);
    if (result.is_err()) {
        return Result<std::string, Error>::err(std::move(result).error());
    }

    return Result<std::string, Error>::ok(result.value().content);
}

// ContextManager
ContextManager::ContextManager(const ContextConfig& config, llm::LLMGateway& llm)
    : config_(config)
    , llm_(llm)
{
    compactor_ = std::make_unique<ContextCompactor>(config, llm);
}

Result<ContextWindow, Error> ContextManager::build_context(
    memory::MemoryManager& memory,
    const std::string& system_prompt,
    const Json& tools,
    const std::string& current_task) {

    ContextBuilder builder(config_);

    builder.with_system_prompt(system_prompt)
           .with_tools(tools);

    // Add user and project memory
    std::string user_mem = memory.get_user_memory();
    std::string project_mem = memory.get_project_memory();

    if (!user_mem.empty()) {
        builder.with_user_memory(user_mem);
    }

    if (!project_mem.empty()) {
        builder.with_project_memory(project_mem);
    }

    // Add compressed history
    std::string history = memory.get_compressed_history();
    if (!history.empty()) {
        builder.with_compressed_history(history);
    }

    // Add recent messages (keep last N raw)
    auto recent = memory.get_recent_turns(config_.keep_raw_turns * 2);  // *2 for user+assistant pairs
    builder.with_messages(recent);

    // Add relevant episodes if we have some
    if (!current_task.empty()) {
        auto episodes = memory.retrieve_episodes(current_task, 3);
        if (!episodes.empty()) {
            builder.with_episodes(episodes);
        }
        builder.with_task_context(current_task);
    }

    return builder.build();
}

Result<void, Error> ContextManager::compact_if_needed(memory::MemoryManager& memory) {
    const auto& thread = memory.thread_memory();
    auto& history = memory.compressed_history();

    // Estimate current token usage
    int tokens = 0;
    for (const auto& msg : thread.messages()) {
        tokens += static_cast<int>(msg.content.length() / 3.5);
    }

    if (!compactor_->needs_compaction(tokens)) {
        return Result<void, Error>::ok();
    }

    // We need to compact
    // Strategy: Summarize older messages, keep recent ones raw
    size_t total_messages = thread.messages().size();
    size_t keep_raw = config_.keep_raw_turns * 2;  // User + assistant pairs

    if (total_messages <= keep_raw) {
        return Result<void, Error>::ok();  // Nothing to compact
    }

    // Find the range to compact
    size_t compact_end = total_messages - keep_raw;
    size_t batch_size = config_.summarize_batch;

    // Compact in batches
    size_t compact_start = 0;
    while (compact_start < compact_end) {
        size_t batch_end = std::min(compact_start + batch_size, compact_end);

        // Get messages for this batch
        auto batch_messages = thread.get_range(compact_start, batch_end);

        // Summarize
        auto summary_result = compactor_->compact_messages(
            std::vector<Message>(batch_messages.begin(), batch_messages.end()),
            0, batch_messages.size()
        );

        if (summary_result.is_ok()) {
            history.add_summary(compact_start, batch_end, std::move(summary_result).value());
        }

        compact_start = batch_end;
    }

    // Trim the thread memory to keep only recent messages
    // Note: This modifies the memory, which should be saved afterward
    memory.thread_memory().trim(keep_raw);

    return Result<void, Error>::ok();
}

int ContextManager::remaining_tokens(int current_tokens) const {
    return config_.max_tokens - config_.reserved_for_response - current_tokens;
}

bool ContextManager::is_near_limit(int current_tokens) const {
    return current_tokens > config_.compaction_threshold;
}

}  // namespace gpagent::context
