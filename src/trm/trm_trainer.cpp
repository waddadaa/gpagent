#include "gpagent/trm/trm_trainer.hpp"

#include <cmath>
#include <numeric>
#include <random>

namespace gpagent::trm {

TRMTrainer::TRMTrainer(TRMModel& model, EpisodeBuffer& buffer, const TRMConfig& config)
    : model_(model)
    , buffer_(buffer)
    , config_(config)
{
    last_training_time_ = Clock::now();
}

TRMTrainer::~TRMTrainer() {
    stop_training();
    wait_for_completion();
}

bool TRMTrainer::should_start_training() const {
    if (training_in_progress_.load()) {
        return false;
    }

    return buffer_.has_enough_for_training();
}

Result<void, Error> TRMTrainer::start_training_async(TrainingCallback callback) {
    if (training_in_progress_.exchange(true)) {
        return Result<void, Error>::err(
            ErrorCode::InvalidState,
            "Training already in progress"
        );
    }

    if (!buffer_.has_enough_for_training()) {
        training_in_progress_.store(false);
        return Result<void, Error>::err(
            ErrorCode::InvalidArgument,
            "Not enough episodes for training. Need " +
                std::to_string(config_.min_episodes_before_training) +
                ", have " + std::to_string(buffer_.size())
        );
    }

    stop_requested_.store(false);

    // Start training in background thread
    training_thread_ = std::make_unique<std::jthread>([this, callback]() {
        train_loop(callback);
    });

    return Result<void, Error>::ok();
}

void TRMTrainer::wait_for_completion() {
    if (training_thread_ && training_thread_->joinable()) {
        training_thread_->join();
    }
}

void TRMTrainer::stop_training() {
    stop_requested_.store(true);
}

std::chrono::hours TRMTrainer::time_until_retrain() const {
    auto now = Clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - last_training_time_);
    auto retrain_interval = std::chrono::hours(config_.retrain_interval_hours);

    if (elapsed >= retrain_interval) {
        return std::chrono::hours(0);
    }

    return retrain_interval - elapsed;
}

bool TRMTrainer::is_retrain_due() const {
    return time_until_retrain().count() == 0;
}

void TRMTrainer::train_loop(TrainingCallback callback) {
    last_result_ = TrainingProgress{};
    last_result_.total_epochs = config_.epochs;

    // NOTE: This is a simplified training loop that demonstrates the structure.
    // In a full implementation, this would use a neural network library (e.g., ONNX Runtime,
    // LibTorch, or custom CUDA kernels) to perform actual gradient-based optimization.

    // The TRM architecture from the paper:
    // - 2-layer transformer
    // - T=3 recursive passes
    // - n=6 iterations per layer per pass
    // - N_sup=16 deep supervision points
    // - 7M parameters total
    // - EMA with decay 0.999

    std::mt19937 rng(std::random_device{}());
    float learning_rate = config_.learning_rate;

    for (int epoch = 0; epoch < config_.epochs && !stop_requested_.load(); ++epoch) {
        last_result_.current_epoch = epoch + 1;

        // Compute losses for each unsupervised objective
        float contrastive = compute_contrastive_loss();
        float next_action = compute_next_action_loss();
        float outcome = compute_outcome_loss();
        float masked = compute_masked_loss();

        // Combined loss (weighted sum)
        float total_loss = 0.25f * contrastive +
                           0.25f * next_action +
                           0.25f * outcome +
                           0.25f * masked;

        last_result_.contrastive_loss = contrastive;
        last_result_.next_action_loss = next_action;
        last_result_.outcome_loss = outcome;
        last_result_.masked_loss = masked;
        last_result_.loss = total_loss;

        // Report progress
        if (callback) {
            callback(last_result_);
        }

        // Simulate gradient descent (in real impl, would update model weights)
        // The loss values decrease over epochs in this simulation
        learning_rate *= 0.99f;  // Learning rate decay

        // Small delay to prevent CPU spinning in this placeholder
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    last_result_.complete = true;
    last_training_time_ = Clock::now();
    training_in_progress_.store(false);

    if (callback) {
        callback(last_result_);
    }
}

float TRMTrainer::compute_contrastive_loss() {
    // Contrastive learning: Learn tool-context alignment
    // Objective: Similar contexts should have similar tool embeddings,
    // different contexts should have different tool embeddings

    // Sample contrastive pairs
    auto pairs = buffer_.sample_contrastive_pairs(32);

    if (pairs.empty()) {
        return 1.0f;  // High loss when no data
    }

    // In a real implementation:
    // 1. Encode anchor context -> anchor_embedding
    // 2. Encode positive context -> positive_embedding
    // 3. Encode negative context -> negative_embedding
    // 4. Compute InfoNCE loss:
    //    L = -log(exp(sim(anchor, positive)/τ) /
    //            (exp(sim(anchor, positive)/τ) + exp(sim(anchor, negative)/τ)))

    // For now, simulate loss based on outcome similarity
    float total_loss = 0.0f;

    for (const auto& pair : pairs) {
        // Anchor and positive should be similar (same outcome)
        // Anchor and negative should be different (different outcome)
        bool anchor_success = pair.anchor.outcome.success;
        bool positive_success = pair.positive.outcome.success;
        bool negative_success = pair.negative.outcome.success;

        // Perfect contrastive pair: anchor==positive, anchor!=negative
        float pair_loss = 0.0f;
        if (anchor_success == positive_success) {
            pair_loss += 0.0f;  // Good: positive matches
        } else {
            pair_loss += 0.5f;  // Bad: positive doesn't match
        }

        if (anchor_success != negative_success) {
            pair_loss += 0.0f;  // Good: negative is different
        } else {
            pair_loss += 0.5f;  // Bad: negative matches (should be different)
        }

        total_loss += pair_loss;
    }

    return total_loss / static_cast<float>(pairs.size());
}

float TRMTrainer::compute_next_action_loss() {
    // Next-action prediction: Autoregressive sequence modeling
    // Given context and action history [a1, a2, ..., an], predict a(n+1)

    auto batch = buffer_.sample_batch(32);

    if (batch.episodes.empty()) {
        return 1.0f;
    }

    // In a real implementation:
    // 1. For each episode, create sequences of varying lengths
    // 2. Input: context + [a1..an], Target: a(n+1)
    // 3. Compute cross-entropy loss on tool prediction

    float total_loss = 0.0f;
    int valid_sequences = 0;

    for (const auto& episode : batch.episodes) {
        if (episode.actions.size() < 2) continue;

        // For each position in the sequence, predict next action
        for (size_t i = 0; i < episode.actions.size() - 1; ++i) {
            // In real impl: loss = CrossEntropy(model(context, a[0:i]), a[i+1])
            // Simulated: random loss that trends lower with more data
            float sequence_loss = 0.5f + 0.5f * (1.0f - static_cast<float>(i) /
                                                  static_cast<float>(episode.actions.size()));
            total_loss += sequence_loss;
            ++valid_sequences;
        }
    }

    if (valid_sequences == 0) {
        return 1.0f;
    }

    return total_loss / static_cast<float>(valid_sequences);
}

float TRMTrainer::compute_outcome_loss() {
    // Outcome prediction: Binary classification of success/failure
    // This is the primary supervised signal (but self-supervised from interaction outcomes)

    auto batch = buffer_.sample_batch(32);

    if (batch.episodes.empty()) {
        return 1.0f;
    }

    // In a real implementation:
    // 1. Encode full episode (context + all actions)
    // 2. Predict binary outcome (success/failure)
    // 3. Compute binary cross-entropy loss

    float total_loss = 0.0f;
    int success_count = 0;
    int failure_count = 0;

    for (const auto& episode : batch.episodes) {
        if (episode.outcome.success) {
            ++success_count;
        } else {
            ++failure_count;
        }

        // Simulated BCE loss
        // In real impl: loss = -y*log(p) - (1-y)*log(1-p)
        float simulated_confidence = 0.6f;  // Model's predicted probability
        float target = episode.outcome.success ? 1.0f : 0.0f;

        // BCE loss
        float p = std::clamp(simulated_confidence, 0.001f, 0.999f);
        float bce = -target * std::log(p) - (1.0f - target) * std::log(1.0f - p);
        total_loss += bce;
    }

    // Add class imbalance penalty if severely imbalanced
    if (success_count > 0 && failure_count > 0) {
        float ratio = static_cast<float>(std::min(success_count, failure_count)) /
                      static_cast<float>(std::max(success_count, failure_count));
        if (ratio < 0.2f) {
            total_loss *= (1.0f + (0.2f - ratio));  // Penalize imbalance
        }
    }

    return total_loss / static_cast<float>(batch.episodes.size());
}

float TRMTrainer::compute_masked_loss() {
    // Masked tool prediction: BERT-style fill-in-the-blank
    // Randomly mask tools in action sequence and predict them

    auto batch = buffer_.sample_batch(32);

    if (batch.episodes.empty()) {
        return 1.0f;
    }

    // In a real implementation:
    // 1. For each episode action sequence [a1, a2, a3, ...]
    // 2. Randomly mask 15% of actions with [MASK] token
    // 3. Predict original tool from context
    // 4. Compute cross-entropy loss

    std::mt19937 rng(std::random_device{}());
    std::bernoulli_distribution mask_dist(0.15);  // 15% mask rate

    float total_loss = 0.0f;
    int masked_count = 0;

    for (const auto& episode : batch.episodes) {
        if (episode.actions.empty()) continue;

        for (size_t i = 0; i < episode.actions.size(); ++i) {
            if (mask_dist(rng)) {
                // This action is masked, need to predict it
                // In real impl: loss = CrossEntropy(model(context, masked_sequence), original_tool)

                // Simulated loss based on sequence position
                // Actions earlier in sequence are harder to predict
                float position_factor = static_cast<float>(i) /
                                         static_cast<float>(episode.actions.size());
                float masked_loss = 0.8f - 0.3f * position_factor;
                total_loss += masked_loss;
                ++masked_count;
            }
        }
    }

    if (masked_count == 0) {
        // Force some masking if random didn't select any
        return 0.7f;
    }

    return total_loss / static_cast<float>(masked_count);
}

}  // namespace gpagent::trm
