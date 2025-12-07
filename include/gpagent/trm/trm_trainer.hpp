#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "trm_model.hpp"
#include "episode_buffer.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace gpagent::trm {

using namespace gpagent::core;

// Training callback for progress updates
using TrainingCallback = std::function<void(const TrainingProgress&)>;

// TRM Trainer - handles unsupervised training
class TRMTrainer {
public:
    TRMTrainer(TRMModel& model, EpisodeBuffer& buffer, const TRMConfig& config);
    ~TRMTrainer();

    // Check if training should start
    bool should_start_training() const;

    // Start training asynchronously
    Result<void, Error> start_training_async(TrainingCallback callback = nullptr);

    // Check if training is in progress
    bool is_training() const { return training_in_progress_.load(); }

    // Wait for training to complete
    void wait_for_completion();

    // Stop training early
    void stop_training();

    // Get last training result
    const TrainingProgress& last_training_result() const { return last_result_; }

    // Get time until next scheduled retraining
    std::chrono::hours time_until_retrain() const;

    // Check if retraining is due
    bool is_retrain_due() const;

private:
    TRMModel& model_;
    EpisodeBuffer& buffer_;
    TRMConfig config_;

    std::atomic<bool> training_in_progress_{false};
    std::atomic<bool> stop_requested_{false};
    std::unique_ptr<std::jthread> training_thread_;

    TrainingProgress last_result_;
    TimePoint last_training_time_;

    // Training implementation
    void train_loop(TrainingCallback callback);

    // Unsupervised training objectives
    float compute_contrastive_loss();
    float compute_next_action_loss();
    float compute_outcome_loss();
    float compute_masked_loss();
};

}  // namespace gpagent::trm
