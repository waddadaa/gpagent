#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "gpagent/memory/episodic_memory.hpp"

#include <deque>
#include <mutex>
#include <random>
#include <vector>

namespace gpagent::trm {

using namespace gpagent::core;

// A training batch sampled from the episode buffer
struct TrainingBatch {
    std::vector<memory::Episode> episodes;
    std::vector<size_t> indices;  // Original indices for tracking
};

// Episode buffer for TRM training
// Stores episodes in memory for efficient sampling during training
class EpisodeBuffer {
public:
    explicit EpisodeBuffer(const TRMConfig& config);

    // Add episode to buffer
    void add_episode(const memory::Episode& episode);

    // Get number of episodes
    size_t size() const;

    // Check if we have enough episodes for training
    bool has_enough_for_training() const;

    // Sample a random batch for training
    TrainingBatch sample_batch(size_t batch_size);

    // Sample contrastive pairs (positive and negative examples)
    struct ContrastivePair {
        memory::Episode anchor;
        memory::Episode positive;   // Same outcome/similar context
        memory::Episode negative;   // Different outcome/different context
    };
    std::vector<ContrastivePair> sample_contrastive_pairs(size_t num_pairs);

    // Get all episodes (for full-batch training)
    const std::deque<memory::Episode>& all_episodes() const { return episodes_; }

    // Clear buffer (after training or for reset)
    void clear();

    // Load episodes from episodic memory
    Result<size_t, Error> load_from_memory(memory::EpisodicMemory& episodic);

    // Get episodes by outcome
    std::vector<memory::Episode> get_successful_episodes() const;
    std::vector<memory::Episode> get_failed_episodes() const;

    // Statistics
    size_t successful_count() const { return successful_count_; }
    size_t failed_count() const { return failed_count_; }
    float success_rate() const;

private:
    TRMConfig config_;
    std::deque<memory::Episode> episodes_;
    size_t successful_count_ = 0;
    size_t failed_count_ = 0;

    mutable std::mutex mutex_;
    std::mt19937 rng_;

    // Maintain max buffer size
    void trim_if_needed();
};

}  // namespace gpagent::trm
