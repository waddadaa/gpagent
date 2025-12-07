#include "gpagent/agent/orchestrator.hpp"

#include <spdlog/spdlog.h>

namespace gpagent::agent {

Orchestrator::Orchestrator(
    const Config& config,
    llm::LLMGateway& llm,
    tools::ToolRegistry& tools,
    tools::ToolExecutor& executor,
    memory::MemoryManager& memory,
    context::ContextManager& context)
    : config_(config)
    , llm_(llm)
    , tools_(tools)
    , executor_(executor)
    , memory_(memory)
    , context_(context)
{
}

Orchestrator::~Orchestrator() {
    shutdown();
}

Result<void, Error> Orchestrator::initialize() {
    // Initialize TRM components
    // TODO: Get TRM config from main config, not memory config
    trm::TRMConfig trm_config;  // Use defaults for now

    trm_model_ = std::make_unique<trm::TRMModel>(trm_config);
    episode_buffer_ = std::make_unique<trm::EpisodeBuffer>(trm_config);
    trm_trainer_ = std::make_unique<trm::TRMTrainer>(*trm_model_, *episode_buffer_, trm_config);

    // Load episodes from episodic memory into buffer
    auto load_result = episode_buffer_->load_from_memory(memory_.episodic_memory());
    if (load_result.is_err()) {
        spdlog::warn("Failed to load episodes into buffer: {}", load_result.error().message);
    } else {
        spdlog::info("Loaded {} episodes into TRM buffer", load_result.value());
    }

    // Try to load existing TRM model
    // TODO: Get model path from config properly
    auto model_path = memory_.config().storage_path / "trm" / "model.bin";
    if (std::filesystem::exists(model_path)) {
        auto load_model = trm_model_->load(model_path);
        if (load_model.is_ok()) {
            spdlog::info("Loaded TRM model from {}", model_path.string());
        }
    }

    state_.store(AgentState::Idle);
    return Result<void, Error>::ok();
}

Result<std::string, Error> Orchestrator::process(
    const std::string& user_input,
    StreamCallback stream_cb) {

    return process_with_events(user_input, nullptr, stream_cb);
}

Result<std::string, Error> Orchestrator::process_with_events(
    const std::string& user_input,
    AgentEventCallback event_cb,
    StreamCallback stream_cb) {

    if (shutdown_requested_.load()) {
        return Result<std::string, Error>::err(
            ErrorCode::InvalidState,
            "Agent is shutting down"
        );
    }

    AgentState expected = AgentState::Idle;
    if (!state_.compare_exchange_strong(expected, AgentState::Processing)) {
        return Result<std::string, Error>::err(
            ErrorCode::InvalidState,
            "Agent is busy"
        );
    }

    // Start new task tracking
    current_task_description_ = user_input;
    current_actions_.clear();
    task_start_time_ = Clock::now();
    current_turn_ = 0;

    // Add user message to memory
    memory_.add_message(Message::user(user_input));

    if (event_cb) {
        event_cb({AgentEvent::Thinking, "Processing request...", {}});
    }

    std::string final_response;
    bool task_complete = false;

    // Main agent loop
    while (!task_complete && current_turn_ < config_.max_turns_per_task) {
        ++current_turn_;

        // Call LLM
        auto llm_result = call_llm(current_task_description_, stream_cb);
        if (llm_result.is_err()) {
            state_.store(AgentState::Idle);
            return Result<std::string, Error>::err(std::move(llm_result).error());
        }

        auto response = std::move(llm_result).value();

        // Check for tool calls
        if (!response.tool_calls.empty()) {
            if (event_cb) {
                Json tools_json = Json::array();
                for (const auto& tc : response.tool_calls) {
                    tools_json.push_back(tc.tool_name);
                }
                event_cb({AgentEvent::ToolSelected, "Tools selected", {{"tools", tools_json}}});
            }

            // IMPORTANT: Save assistant message with tool_calls to memory BEFORE executing tools
            // This ensures tool_result messages have a corresponding tool_use in the conversation
            Message assistant_msg = Message::assistant(response.content);
            assistant_msg.tool_calls = response.tool_calls;
            memory_.add_message(assistant_msg);
            spdlog::info("Saved assistant message with {} tool calls to memory", response.tool_calls.size());

            // Execute tools
            state_.store(AgentState::ExecutingTool);
            auto exec_result = execute_tool_calls(response.tool_calls, event_cb);
            state_.store(AgentState::Processing);

            if (exec_result.is_err()) {
                spdlog::error("Tool execution failed: {}", exec_result.error().message);
                // Continue loop to let LLM handle the error
            }
        } else {
            // No tool calls - this is the final response
            final_response = response.content;
            task_complete = true;
        }

        // Check for stop condition in content
        if (response.stop_reason == StopReason::EndTurn && response.tool_calls.empty()) {
            task_complete = true;
            final_response = response.content;
        }
    }

    // Add assistant response to memory
    if (!final_response.empty()) {
        memory_.add_message(Message::assistant(final_response));
    }

    // Check if we hit turn limit
    if (current_turn_ >= config_.max_turns_per_task) {
        spdlog::warn("Task hit maximum turn limit ({})", config_.max_turns_per_task);
    }

    if (event_cb) {
        event_cb({AgentEvent::ResponseReady, final_response, {}});
    }

    // Check if we should start TRM training
    if (config_.auto_train_trm) {
        check_and_start_training(event_cb);
    }

    state_.store(AgentState::Idle);
    return Result<std::string, Error>::ok(std::move(final_response));
}

void Orchestrator::shutdown() {
    shutdown_requested_.store(true);

    // Stop any ongoing training
    if (trm_trainer_) {
        trm_trainer_->stop_training();
        trm_trainer_->wait_for_completion();
    }

    state_.store(AgentState::Shutdown);
}

Result<void, Error> Orchestrator::trigger_training() {
    if (!trm_trainer_) {
        return Result<void, Error>::err(
            ErrorCode::InvalidState,
            "TRM trainer not initialized"
        );
    }

    return trm_trainer_->start_training_async([](const trm::TrainingProgress& progress) {
        spdlog::info("TRM Training: epoch {}/{}, loss {:.4f}",
                     progress.current_epoch, progress.total_epochs, progress.loss);
    });
}

void Orchestrator::complete_task(bool success, const std::string& summary) {
    finalize_episode(success, summary);

    // Save memory state
    auto save_result = memory_.save_all();
    if (save_result.is_err()) {
        spdlog::error("Failed to save memory: {}", save_result.error().message);
    }
}

void Orchestrator::abort_task() {
    finalize_episode(false, "Task aborted by user");
    current_actions_.clear();
    current_task_description_.clear();
}

Result<LLMResponse, Error> Orchestrator::call_llm(
    const std::string& task,
    StreamCallback stream_cb) {

    // Build context window
    std::string system_prompt = config_.system_prompt;

    // Augment with TRM recommendations if available
    spdlog::info("TRM status: use_recommendations={}, model_ready={}",
                 config_.use_trm_recommendations, trm_model_->is_ready());
    if (config_.use_trm_recommendations && trm_model_->is_ready()) {
        system_prompt += augment_system_prompt_with_trm();
    }

    auto context_result = context_.build_context(
        memory_,
        system_prompt,
        build_tool_schemas(),
        task
    );

    if (context_result.is_err()) {
        return Result<LLMResponse, Error>::err(std::move(context_result).error());
    }

    auto context_window = std::move(context_result).value();

    // Build LLM request
    llm::LLMRequest request;
    request.system_prompt = context_window.system_prompt;
    request.messages = context_window.messages;
    request.tools = context_window.tools;
    request.max_tokens = 4096;
    request.temperature = 0.7f;

    if (stream_cb) {
        request.stream_callback = stream_cb;
    }

    // Call primary LLM
    return llm_.primary().complete(request);
}

Result<void, Error> Orchestrator::execute_tool_calls(
    const std::vector<ToolCall>& calls,
    AgentEventCallback event_cb) {

    for (const auto& call : calls) {
        if (event_cb) {
            event_cb({
                AgentEvent::ToolExecuting,
                "Executing " + call.tool_name,
                {{"tool", call.tool_name}, {"args", call.arguments}}
            });
        }

        // Create tool context
        tools::ToolContext ctx;
        ctx.working_directory = std::filesystem::current_path().string();
        ctx.timeout_ms = 120000;  // 2 minutes
        ctx.config = app_config_;  // Pass app config to tools

        // Set allowed paths for sandbox - include home directory and common locations
        const char* home = std::getenv("HOME");
        if (home) {
            ctx.allowed_paths.push_back(home);
        }
        ctx.allowed_paths.push_back(ctx.working_directory);
        ctx.allowed_paths.push_back("/tmp");

        // Execute the tool
        auto result = executor_.execute(call, ctx);

        bool success = result.is_ok();
        std::string output = success ? result.value().content : result.error().message;
        bool is_image_result = success && result.value().is_image;

        spdlog::info("Tool {} result: success={}, is_image={}, output_len={}",
                     call.tool_name, success, is_image_result, output.size());

        // Record the action for episode tracking
        record_action(call.tool_name, call.arguments, output, success);

        // Add tool result to memory
        Message tool_msg = Message::tool_result(call.id, output);

        // If this is an image result, parse the JSON and add image data
        if (is_image_result) {
            spdlog::info("Processing image result...");
            try {
                Json img_json = Json::parse(output);
                spdlog::info("Parsed image JSON, has data={}, has media_type={}",
                             img_json.contains("data"), img_json.contains("media_type"));
                if (img_json.contains("data") && img_json.contains("media_type")) {
                    ImageContent img;
                    img.data = img_json["data"].get<std::string>();
                    img.media_type = img_json["media_type"].get<std::string>();
                    img.source_path = img_json.value("file_path", "");
                    tool_msg.images.push_back(std::move(img));
                    // Set content to a descriptive text instead of the base64 blob
                    tool_msg.content = "Image loaded from: " + img_json.value("file_path", "unknown");
                    spdlog::info("Added image to tool result: {} (data_len={})",
                                 img.source_path, tool_msg.images.back().data.size());
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to parse image result: {}", e.what());
            }
        }

        spdlog::info("Tool message content_len={}, images_count={}",
                     tool_msg.content.size(), tool_msg.images.size());
        memory_.add_message(tool_msg);

        if (event_cb) {
            auto event = success ? AgentEvent::ToolCompleted : AgentEvent::ToolFailed;
            event_cb({
                event,
                output,
                {{"tool", call.tool_name}, {"success", success}}
            });
        }
    }

    return Result<void, Error>::ok();
}

void Orchestrator::record_action(
    const std::string& tool,
    const Json& args,
    const std::string& result,
    bool success) {

    memory::EpisodeAction action;
    action.tool = tool;
    action.arguments = args;
    action.result_summary = result.substr(0, 500);  // Truncate for storage
    action.success = success;
    action.timestamp = Clock::now();

    current_actions_.push_back(std::move(action));
}

void Orchestrator::finalize_episode(bool success, const std::string& summary) {
    if (current_task_description_.empty()) {
        return;  // No active episode
    }

    // Create episode
    memory::Episode episode;
    episode.id = gpagent::core::generate_episode_id();
    episode.task_description = current_task_description_;
    episode.actions = std::move(current_actions_);
    episode.outcome.success = success;
    episode.outcome.summary = summary;
    episode.outcome.duration = std::chrono::duration_cast<std::chrono::seconds>(
        Clock::now() - task_start_time_
    );
    episode.started_at = task_start_time_;
    episode.completed_at = Clock::now();

    // Add to episodic memory
    memory_.episodic_memory().add_episode(episode);

    // Add to TRM training buffer
    if (episode_buffer_) {
        episode_buffer_->add_episode(episode);
    }

    // Clear current task tracking
    current_task_description_.clear();
    current_actions_.clear();

    spdlog::info("Episode completed: {} ({})",
                 episode.id,
                 success ? "success" : "failure");
}

void Orchestrator::check_and_start_training(AgentEventCallback event_cb) {
    if (!trm_trainer_ || !episode_buffer_) {
        return;
    }

    if (trm_trainer_->should_start_training()) {
        spdlog::info("Starting TRM training with {} episodes", episode_buffer_->size());

        if (event_cb) {
            event_cb({AgentEvent::TrainingStarted, "TRM training started", {}});
        }

        state_.store(AgentState::Training);

        auto training_cb = [event_cb](const trm::TrainingProgress& progress) {
            if (event_cb) {
                event_cb({
                    AgentEvent::TrainingProgress,
                    "Training progress",
                    {
                        {"epoch", progress.current_epoch},
                        {"total_epochs", progress.total_epochs},
                        {"loss", progress.loss}
                    }
                });
            }
        };

        auto result = trm_trainer_->start_training_async(training_cb);
        if (result.is_err()) {
            spdlog::error("Failed to start training: {}", result.error().message);
        }

        // Note: Training runs async, state will be reset when processing completes
    }
}

Json Orchestrator::build_tool_schemas() const {
    Json schemas = Json::array();

    for (const auto& [name, registered_tool] : tools_.all_tools()) {
        if (!registered_tool.enabled) continue;

        const auto& spec = registered_tool.spec;
        Json tool;
        tool["name"] = name;
        tool["description"] = spec.description;

        Json params;
        params["type"] = "object";
        params["properties"] = Json::object();
        params["required"] = Json::array();

        for (const auto& param : spec.parameters) {
            Json param_schema;
            param_schema["type"] = std::string(tools::param_type_to_string(param.type));
            param_schema["description"] = param.description;

            if (param.enum_values && !param.enum_values->empty()) {
                param_schema["enum"] = *param.enum_values;
            }

            params["properties"][param.name] = param_schema;

            if (param.required) {
                params["required"].push_back(param.name);
            }
        }

        tool["input_schema"] = params;
        schemas.push_back(tool);
    }

    return schemas;
}

std::string Orchestrator::augment_system_prompt_with_trm() const {
    std::ostringstream ss;

    // Get tool list for prediction
    std::vector<std::string> tool_names;
    for (const auto& [name, _] : tools_.all_tools()) {
        if (tools_.is_enabled(name)) {
            tool_names.push_back(name);
        }
    }

    spdlog::info("TRM prediction requested for task: {}", current_task_description_.substr(0, 50));

    // Get TRM prediction
    auto prediction = trm_model_->predict(
        current_task_description_,
        tool_names,
        current_actions_
    );

    if (prediction) {
        spdlog::info("TRM prediction: {} (confidence: {:.1f}%)",
                     prediction->recommended_tool, prediction->confidence * 100);
    } else {
        spdlog::info("TRM prediction: no prediction available");
    }

    if (prediction && prediction->confidence > 0.5f) {
        ss << "\n\n## TRM Suggestion\n";
        ss << "Based on similar past tasks, consider using: " << prediction->recommended_tool;
        ss << " (confidence: " << static_cast<int>(prediction->confidence * 100) << "%)\n";

        if (prediction->ranked_tools.size() > 1) {
            ss << "Alternative tools: ";
            for (size_t i = 1; i < std::min(size_t(3), prediction->ranked_tools.size()); ++i) {
                if (i > 1) ss << ", ";
                ss << prediction->ranked_tools[i].first;
            }
            ss << "\n";
        }
    }

    return ss.str();
}

}  // namespace gpagent::agent
