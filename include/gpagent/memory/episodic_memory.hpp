#pragma once

#include "gpagent/core/types.hpp"
#include "gpagent/core/result.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gpagent::memory {

using namespace gpagent::core;
namespace fs = std::filesystem;

// Action taken during an episode
struct EpisodeAction {
    ToolId tool;
    Json arguments;
    bool success;
    std::optional<std::string> error;
    std::string result_summary;
    Duration execution_time{0};
    TimePoint timestamp;

    Json to_json() const;
    static EpisodeAction from_json(const Json& j);
};

// Outcome of an episode
struct EpisodeOutcome {
    bool success = false;
    int turns_taken = 0;
    int tools_used = 0;
    Duration total_time{0};
    std::chrono::seconds duration{0};
    std::string summary;
    std::optional<std::string> failure_reason;

    Json to_json() const;
    static EpisodeOutcome from_json(const Json& j);
};

// Episode - a complete task interaction for learning
struct Episode {
    EpisodeId id;
    TimePoint timestamp;
    TimePoint started_at;
    TimePoint completed_at;

    // Task information
    std::string task_description;
    std::string task_category;  // bug_fix, feature, refactor, etc.

    // Context at start
    std::string project;
    std::vector<std::string> files_involved;
    std::optional<std::string> initial_context;

    // Actions taken
    std::vector<EpisodeAction> actions;

    // Outcome
    EpisodeOutcome outcome;

    // Learnings (extracted patterns)
    std::vector<std::string> learnings;

    // For retrieval
    std::vector<std::string> keywords;

    // Serialization
    Json to_json() const;
    static Episode from_json(const Json& j);
};

// Episode index for fast retrieval
struct EpisodeIndexEntry {
    EpisodeId id;
    std::vector<std::string> keywords;
    std::string category;
    bool success;
    TimePoint timestamp;
    int turns;

    Json to_json() const;
    static EpisodeIndexEntry from_json(const Json& j);
};

// Episodic memory - stores and retrieves past experiences
class EpisodicMemory {
public:
    explicit EpisodicMemory(const fs::path& storage_path);

    // Store a new episode
    Result<void, Error> store(const Episode& episode);

    // Add episode (alias for store)
    void add_episode(const Episode& episode) { store(episode); }

    // Retrieve episode by ID
    Result<Episode, Error> get(const EpisodeId& id) const;

    // Get all episodes
    std::vector<Episode> all_episodes() const;

    // Search for relevant episodes
    std::vector<Episode> search(const std::string& query, size_t limit = 5) const;

    // Search by category
    std::vector<Episode> search_by_category(const std::string& category, size_t limit = 5) const;

    // Get recent episodes
    std::vector<Episode> get_recent(size_t limit = 10) const;

    // Get successful episodes (for training)
    std::vector<Episode> get_successful(size_t limit = 100) const;

    // Count total episodes
    size_t count() const;

    // Count successful episodes
    size_t count_successful() const;

    // Get episode count (alias)
    size_t episode_count() const { return count(); }

    // Load index from disk
    Result<void, Error> load_index();

    // Save index to disk
    Result<void, Error> save_index() const;

private:
    fs::path storage_path_;
    fs::path index_path_;
    std::vector<EpisodeIndexEntry> index_;

    // Get episode file path
    fs::path episode_path(const EpisodeId& id) const;

    // Update index with new episode
    void update_index(const Episode& episode);

    // Simple keyword matching score
    float keyword_score(const std::vector<std::string>& episode_keywords,
                        const std::vector<std::string>& query_keywords) const;

    // Extract keywords from text
    static std::vector<std::string> extract_keywords(const std::string& text);
};

}  // namespace gpagent::memory
