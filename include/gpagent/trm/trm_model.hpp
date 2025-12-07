#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "gpagent/core/types.hpp"
#include "gpagent/memory/episodic_memory.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gpagent::trm {

using namespace gpagent::core;
namespace fs = std::filesystem;

// TRM prediction result
struct TRMPrediction {
    ToolId recommended_tool;
    float confidence;
    std::vector<std::pair<ToolId, float>> ranked_tools;  // Top-k tools with scores
};

// Training progress
struct TrainingProgress {
    int current_epoch = 0;
    int total_epochs = 0;
    float loss = 0.0f;
    float contrastive_loss = 0.0f;
    float next_action_loss = 0.0f;
    float outcome_loss = 0.0f;
    float masked_loss = 0.0f;
    bool complete = false;
};

// TRM model status
enum class TRMStatus {
    NotInitialized,      // Model not loaded
    ColdStart,           // Collecting data, using fallback
    Training,            // Currently training
    Ready,               // Model ready for inference
    Fallback             // Using fallback due to error
};

// TRM Model interface (can be implemented with different backends)
class TRMModel {
public:
    explicit TRMModel(const TRMConfig& config);
    virtual ~TRMModel() = default;

    // Get model status
    TRMStatus status() const { return status_; }

    // Check if model is ready for inference (includes ColdStart with fallback)
    bool is_ready() const {
        return status_ == TRMStatus::Ready || status_ == TRMStatus::ColdStart;
    }

    // Check if we have enough data to start training
    bool can_start_training(size_t episode_count) const;

    // Predict tool for given context (returns nullopt if not ready)
    virtual std::optional<TRMPrediction> predict(
        const std::string& task_context,
        const std::vector<std::string>& available_tools,
        const std::vector<memory::EpisodeAction>& history = {}
    );

    // Fallback prediction (rule-based)
    TRMPrediction fallback_predict(
        const std::string& task_context,
        const std::vector<std::string>& available_tools
    );

    // Load model from disk
    Result<void, Error> load(const fs::path& path);

    // Save model to disk
    Result<void, Error> save(const fs::path& path) const;

    // Get training progress
    const TrainingProgress& training_progress() const { return training_progress_; }

protected:
    TRMConfig config_;
    TRMStatus status_ = TRMStatus::NotInitialized;
    TrainingProgress training_progress_;

    // Keyword-based tool matching for fallback
    static std::vector<std::pair<ToolId, float>> keyword_match(
        const std::string& query,
        const std::vector<std::string>& tools
    );
};

}  // namespace gpagent::trm
