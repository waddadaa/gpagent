#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "gpagent/core/types.hpp"
#include "gpagent/core/uuid.hpp"
#include "gpagent/context/context_manager.hpp"
#include "gpagent/llm/llm_gateway.hpp"
#include "gpagent/memory/memory_manager.hpp"
#include "gpagent/tools/tool_executor.hpp"
#include "gpagent/tools/tool_registry.hpp"
#include "gpagent/trm/episode_buffer.hpp"
#include "gpagent/trm/trm_model.hpp"
#include "gpagent/trm/trm_trainer.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace gpagent::agent {

using namespace gpagent::core;

// Events emitted during agent execution
enum class AgentEvent {
    Thinking,           // Agent is processing
    ToolSelected,       // Tool was selected
    ToolExecuting,      // Tool is executing
    ToolCompleted,      // Tool completed
    ToolFailed,         // Tool failed
    ResponseReady,      // Response to user ready
    EpisodeComplete,    // Task episode completed
    TrainingStarted,    // TRM training started
    TrainingProgress,   // TRM training progress
    TrainingComplete,   // TRM training completed
    Error               // Error occurred
};

// Callback for agent events
struct AgentEventData {
    AgentEvent event;
    std::string message;
    Json metadata;
};

using AgentEventCallback = std::function<void(const AgentEventData&)>;

// Streaming callback for LLM responses
using StreamCallback = std::function<void(const std::string& chunk)>;

// Agent state
enum class AgentState {
    Idle,               // Waiting for input
    Processing,         // Processing user input
    ExecutingTool,      // Executing a tool
    Training,           // TRM is training
    Responding,         // Generating response
    Shutdown            // Agent is shutting down
};

// The main agent orchestrator
// Coordinates LLM, tools, memory, and TRM for agentic task execution
class Orchestrator {
public:
    struct Config {
        int max_turns_per_task = 50;        // Max tool calls per task
        int max_retries = 3;                 // Retries on transient errors
        bool auto_train_trm = true;          // Auto-start TRM training
        bool use_trm_recommendations = true; // Use TRM for tool selection hints
        std::string system_prompt;           // Base system prompt
    };

    Orchestrator(
        const Config& config,
        llm::LLMGateway& llm,
        tools::ToolRegistry& tools,
        tools::ToolExecutor& executor,
        memory::MemoryManager& memory,
        context::ContextManager& context
    );

    ~Orchestrator();

    // Initialize the orchestrator
    Result<void, Error> initialize();

    // Process user input and return response
    Result<std::string, Error> process(
        const std::string& user_input,
        StreamCallback stream_cb = nullptr
    );

    // Process with event callbacks
    Result<std::string, Error> process_with_events(
        const std::string& user_input,
        AgentEventCallback event_cb,
        StreamCallback stream_cb = nullptr
    );

    // Get current state
    AgentState state() const { return state_.load(); }

    // Check if busy
    bool is_busy() const { return state_.load() != AgentState::Idle; }

    // Request shutdown
    void shutdown();

    // Get the TRM model (for status inspection)
    trm::TRMModel& trm_model() { return *trm_model_; }

    // Get episode buffer (for training inspection)
    trm::EpisodeBuffer& episode_buffer() { return *episode_buffer_; }

    // Force TRM training (if enough episodes)
    Result<void, Error> trigger_training();

    // Mark current task as complete
    void complete_task(bool success, const std::string& summary = "");

    // Abort current task
    void abort_task();

    // Set application config (for tool access to API keys, etc.)
    void set_app_config(const core::Config* config) { app_config_ = config; }

private:
    // Pointer to application-wide config (for API keys, search settings, etc.)
    const core::Config* app_config_ = nullptr;
    Config config_;
    llm::LLMGateway& llm_;
    tools::ToolRegistry& tools_;
    tools::ToolExecutor& executor_;
    memory::MemoryManager& memory_;
    context::ContextManager& context_;

    std::atomic<AgentState> state_{AgentState::Idle};
    std::atomic<bool> shutdown_requested_{false};

    // TRM components
    std::unique_ptr<trm::TRMModel> trm_model_;
    std::unique_ptr<trm::EpisodeBuffer> episode_buffer_;
    std::unique_ptr<trm::TRMTrainer> trm_trainer_;

    // Current task tracking
    std::string current_task_description_;
    std::vector<memory::EpisodeAction> current_actions_;
    TimePoint task_start_time_;
    int current_turn_ = 0;

    // Internal methods
    Result<LLMResponse, Error> call_llm(
        const std::string& task,
        StreamCallback stream_cb
    );

    Result<void, Error> execute_tool_calls(
        const std::vector<ToolCall>& calls,
        AgentEventCallback event_cb
    );

    void record_action(
        const std::string& tool,
        const Json& args,
        const std::string& result,
        bool success
    );

    void finalize_episode(bool success, const std::string& summary);

    void check_and_start_training(AgentEventCallback event_cb);

    Json build_tool_schemas() const;

    std::string augment_system_prompt_with_trm() const;
};

}  // namespace gpagent::agent
