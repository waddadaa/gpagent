#include "gpagent/trm/episode_buffer.hpp"

#include <algorithm>
#include <chrono>

namespace gpagent::trm {

EpisodeBuffer::EpisodeBuffer(const TRMConfig& config)
    : config_(config)
    , rng_(std::random_device{}())
{
}

void EpisodeBuffer::add_episode(const memory::Episode& episode) {
    std::lock_guard lock(mutex_);

    episodes_.push_back(episode);

    if (episode.outcome.success) {
        ++successful_count_;
    } else {
        ++failed_count_;
    }

    trim_if_needed();
}

size_t EpisodeBuffer::size() const {
    std::lock_guard lock(mutex_);
    return episodes_.size();
}

bool EpisodeBuffer::has_enough_for_training() const {
    std::lock_guard lock(mutex_);
    return episodes_.size() >= static_cast<size_t>(config_.min_episodes_before_training);
}

TrainingBatch EpisodeBuffer::sample_batch(size_t batch_size) {
    std::lock_guard lock(mutex_);

    TrainingBatch batch;

    if (episodes_.empty()) {
        return batch;
    }

    // Sample with replacement if batch_size > episodes
    size_t actual_batch = std::min(batch_size, episodes_.size());

    // Create index array and shuffle
    std::vector<size_t> indices(episodes_.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng_);

    // Take first batch_size elements
    for (size_t i = 0; i < actual_batch; ++i) {
        batch.indices.push_back(indices[i]);
        batch.episodes.push_back(episodes_[indices[i]]);
    }

    return batch;
}

std::vector<EpisodeBuffer::ContrastivePair> EpisodeBuffer::sample_contrastive_pairs(size_t num_pairs) {
    std::lock_guard lock(mutex_);

    std::vector<ContrastivePair> pairs;

    if (episodes_.size() < 3) {
        return pairs;  // Need at least 3 episodes for contrastive learning
    }

    // Separate successful and failed episodes
    std::vector<size_t> success_indices;
    std::vector<size_t> failure_indices;

    for (size_t i = 0; i < episodes_.size(); ++i) {
        if (episodes_[i].outcome.success) {
            success_indices.push_back(i);
        } else {
            failure_indices.push_back(i);
        }
    }

    // We need both success and failure examples for good contrastive learning
    if (success_indices.empty() || failure_indices.empty()) {
        // Fall back to using tool diversity for contrast
        for (size_t p = 0; p < num_pairs && p < episodes_.size() / 3; ++p) {
            std::uniform_int_distribution<size_t> dist(0, episodes_.size() - 1);

            ContrastivePair pair;
            size_t anchor_idx = dist(rng_);
            pair.anchor = episodes_[anchor_idx];

            // Find similar episode (shares tools)
            size_t pos_idx = anchor_idx;
            for (size_t attempt = 0; attempt < 10 && pos_idx == anchor_idx; ++attempt) {
                pos_idx = dist(rng_);
            }
            pair.positive = episodes_[pos_idx];

            // Find different episode
            size_t neg_idx = anchor_idx;
            for (size_t attempt = 0; attempt < 10; ++attempt) {
                neg_idx = dist(rng_);
                if (neg_idx != anchor_idx && neg_idx != pos_idx) break;
            }
            pair.negative = episodes_[neg_idx];

            pairs.push_back(std::move(pair));
        }
        return pairs;
    }

    // Sample contrastive pairs based on outcome
    for (size_t p = 0; p < num_pairs; ++p) {
        ContrastivePair pair;

        // Randomly pick anchor from success or failure
        bool anchor_success = std::bernoulli_distribution(0.5)(rng_);
        const auto& anchor_pool = anchor_success ? success_indices : failure_indices;
        const auto& positive_pool = anchor_success ? success_indices : failure_indices;
        const auto& negative_pool = anchor_success ? failure_indices : success_indices;

        if (anchor_pool.empty() || negative_pool.empty()) continue;

        std::uniform_int_distribution<size_t> anchor_dist(0, anchor_pool.size() - 1);
        std::uniform_int_distribution<size_t> neg_dist(0, negative_pool.size() - 1);

        size_t anchor_idx = anchor_pool[anchor_dist(rng_)];
        pair.anchor = episodes_[anchor_idx];

        // Positive: same outcome category
        size_t pos_pool_idx = anchor_dist(rng_);
        if (positive_pool.size() > 1) {
            while (positive_pool[pos_pool_idx] == anchor_idx) {
                pos_pool_idx = anchor_dist(rng_);
            }
        }
        pair.positive = episodes_[positive_pool[pos_pool_idx]];

        // Negative: different outcome category
        pair.negative = episodes_[negative_pool[neg_dist(rng_)]];

        pairs.push_back(std::move(pair));
    }

    return pairs;
}

void EpisodeBuffer::clear() {
    std::lock_guard lock(mutex_);
    episodes_.clear();
    successful_count_ = 0;
    failed_count_ = 0;
}

Result<size_t, Error> EpisodeBuffer::load_from_memory(memory::EpisodicMemory& episodic) {
    std::lock_guard lock(mutex_);

    // Load all episodes from episodic memory
    auto all = episodic.all_episodes();

    size_t loaded = 0;
    for (const auto& ep : all) {
        episodes_.push_back(ep);
        if (ep.outcome.success) {
            ++successful_count_;
        } else {
            ++failed_count_;
        }
        ++loaded;
    }

    trim_if_needed();

    return Result<size_t, Error>::ok(loaded);
}

std::vector<memory::Episode> EpisodeBuffer::get_successful_episodes() const {
    std::lock_guard lock(mutex_);

    std::vector<memory::Episode> result;
    for (const auto& ep : episodes_) {
        if (ep.outcome.success) {
            result.push_back(ep);
        }
    }
    return result;
}

std::vector<memory::Episode> EpisodeBuffer::get_failed_episodes() const {
    std::lock_guard lock(mutex_);

    std::vector<memory::Episode> result;
    for (const auto& ep : episodes_) {
        if (!ep.outcome.success) {
            result.push_back(ep);
        }
    }
    return result;
}

float EpisodeBuffer::success_rate() const {
    std::lock_guard lock(mutex_);

    size_t total = successful_count_ + failed_count_;
    if (total == 0) return 0.0f;
    return static_cast<float>(successful_count_) / static_cast<float>(total);
}

void EpisodeBuffer::trim_if_needed() {
    // Keep max 10x the training threshold to avoid unbounded growth
    size_t max_size = static_cast<size_t>(config_.min_episodes_before_training) * 10;

    while (episodes_.size() > max_size) {
        // Remove oldest episode
        const auto& removed = episodes_.front();
        if (removed.outcome.success) {
            --successful_count_;
        } else {
            --failed_count_;
        }
        episodes_.pop_front();
    }
}

}  // namespace gpagent::trm
