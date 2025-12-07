#include "gpagent/memory/episodic_memory.hpp"
#include "gpagent/core/uuid.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace gpagent::memory {

// EpisodeAction
Json EpisodeAction::to_json() const {
    Json j{
        {"tool", tool},
        {"arguments", arguments},
        {"success", success},
        {"execution_time_ms", execution_time.count()}
    };
    if (error) {
        j["error"] = *error;
    }
    return j;
}

EpisodeAction EpisodeAction::from_json(const Json& j) {
    EpisodeAction action;
    action.tool = j.value("tool", "");
    action.arguments = j.value("arguments", Json::object());
    action.success = j.value("success", true);
    action.execution_time = Duration{j.value("execution_time_ms", 0)};
    if (j.contains("error")) {
        action.error = j["error"].get<std::string>();
    }
    return action;
}

// EpisodeOutcome
Json EpisodeOutcome::to_json() const {
    Json j{
        {"success", success},
        {"turns_taken", turns_taken},
        {"tools_used", tools_used},
        {"total_time_ms", total_time.count()}
    };
    if (failure_reason) {
        j["failure_reason"] = *failure_reason;
    }
    return j;
}

EpisodeOutcome EpisodeOutcome::from_json(const Json& j) {
    EpisodeOutcome outcome;
    outcome.success = j.value("success", false);
    outcome.turns_taken = j.value("turns_taken", 0);
    outcome.tools_used = j.value("tools_used", 0);
    outcome.total_time = Duration{j.value("total_time_ms", 0)};
    if (j.contains("failure_reason")) {
        outcome.failure_reason = j["failure_reason"].get<std::string>();
    }
    return outcome;
}

// Episode
Json Episode::to_json() const {
    Json j{
        {"id", id},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            timestamp.time_since_epoch()).count()},
        {"task_description", task_description},
        {"task_category", task_category},
        {"project", project},
        {"files_involved", files_involved},
        {"outcome", outcome.to_json()},
        {"learnings", learnings},
        {"keywords", keywords}
    };

    if (initial_context) {
        j["initial_context"] = *initial_context;
    }

    Json actions_json = Json::array();
    for (const auto& action : actions) {
        actions_json.push_back(action.to_json());
    }
    j["actions"] = actions_json;

    return j;
}

Episode Episode::from_json(const Json& j) {
    Episode ep;
    ep.id = j.value("id", "");
    ep.task_description = j.value("task_description", "");
    ep.task_category = j.value("task_category", "");
    ep.project = j.value("project", "");

    if (j.contains("timestamp")) {
        ep.timestamp = TimePoint{std::chrono::seconds{j["timestamp"].get<int64_t>()}};
    }

    if (j.contains("files_involved")) {
        ep.files_involved = j["files_involved"].get<std::vector<std::string>>();
    }

    if (j.contains("initial_context")) {
        ep.initial_context = j["initial_context"].get<std::string>();
    }

    if (j.contains("actions")) {
        for (const auto& action_j : j["actions"]) {
            ep.actions.push_back(EpisodeAction::from_json(action_j));
        }
    }

    if (j.contains("outcome")) {
        ep.outcome = EpisodeOutcome::from_json(j["outcome"]);
    }

    if (j.contains("learnings")) {
        ep.learnings = j["learnings"].get<std::vector<std::string>>();
    }

    if (j.contains("keywords")) {
        ep.keywords = j["keywords"].get<std::vector<std::string>>();
    }

    return ep;
}

// EpisodeIndexEntry
Json EpisodeIndexEntry::to_json() const {
    return Json{
        {"id", id},
        {"keywords", keywords},
        {"category", category},
        {"success", success},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            timestamp.time_since_epoch()).count()},
        {"turns", turns}
    };
}

EpisodeIndexEntry EpisodeIndexEntry::from_json(const Json& j) {
    EpisodeIndexEntry entry;
    entry.id = j.value("id", "");
    entry.category = j.value("category", "");
    entry.success = j.value("success", false);
    entry.turns = j.value("turns", 0);

    if (j.contains("keywords")) {
        entry.keywords = j["keywords"].get<std::vector<std::string>>();
    }

    if (j.contains("timestamp")) {
        entry.timestamp = TimePoint{std::chrono::seconds{j["timestamp"].get<int64_t>()}};
    }

    return entry;
}

// EpisodicMemory
EpisodicMemory::EpisodicMemory(const fs::path& storage_path)
    : storage_path_(storage_path)
    , index_path_(storage_path / "index.json")
{
    fs::create_directories(storage_path_);
    load_index();
}

fs::path EpisodicMemory::episode_path(const EpisodeId& id) const {
    return storage_path_ / (id + ".json");
}

Result<void, Error> EpisodicMemory::store(const Episode& episode) {
    try {
        fs::path path = episode_path(episode.id);

        std::ofstream file(path);
        if (!file) {
            return Result<void, Error>::err(
                ErrorCode::FileWriteFailed,
                "Failed to open episode file for writing",
                path.string()
            );
        }

        file << episode.to_json().dump(2);
        update_index(episode);
        save_index();

        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what()
        );
    }
}

Result<Episode, Error> EpisodicMemory::get(const EpisodeId& id) const {
    try {
        fs::path path = episode_path(id);

        if (!fs::exists(path)) {
            return Result<Episode, Error>::err(
                ErrorCode::EpisodeNotFound,
                "Episode not found",
                id
            );
        }

        std::ifstream file(path);
        if (!file) {
            return Result<Episode, Error>::err(
                ErrorCode::FileReadFailed,
                "Failed to open episode file",
                path.string()
            );
        }

        Json j = Json::parse(file);
        return Result<Episode, Error>::ok(Episode::from_json(j));

    } catch (const Json::exception& e) {
        return Result<Episode, Error>::err(
            ErrorCode::MemoryCorrupted,
            std::string("JSON parse error: ") + e.what(),
            id
        );
    } catch (const std::exception& e) {
        return Result<Episode, Error>::err(
            ErrorCode::FileReadFailed,
            e.what(),
            id
        );
    }
}

std::vector<std::string> EpisodicMemory::extract_keywords(const std::string& text) {
    std::vector<std::string> keywords;
    std::unordered_set<std::string> seen;

    std::istringstream iss(text);
    std::string word;

    // Common stop words to skip
    static const std::unordered_set<std::string> stop_words = {
        "the", "a", "an", "is", "are", "was", "were", "be", "been",
        "to", "of", "in", "for", "on", "with", "at", "by", "from",
        "it", "this", "that", "these", "those", "i", "you", "we",
        "and", "or", "but", "if", "then", "else", "when", "while"
    };

    while (iss >> word) {
        // Convert to lowercase
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);

        // Remove punctuation
        word.erase(std::remove_if(word.begin(), word.end(),
            [](char c) { return !std::isalnum(c); }), word.end());

        // Skip short words and stop words
        if (word.length() < 3 || stop_words.count(word) || seen.count(word)) {
            continue;
        }

        seen.insert(word);
        keywords.push_back(word);
    }

    return keywords;
}

float EpisodicMemory::keyword_score(const std::vector<std::string>& episode_keywords,
                                     const std::vector<std::string>& query_keywords) const {
    if (episode_keywords.empty() || query_keywords.empty()) {
        return 0.0f;
    }

    std::unordered_set<std::string> ep_set(episode_keywords.begin(), episode_keywords.end());
    int matches = 0;

    for (const auto& kw : query_keywords) {
        if (ep_set.count(kw)) {
            ++matches;
        }
    }

    return static_cast<float>(matches) / query_keywords.size();
}

std::vector<Episode> EpisodicMemory::search(const std::string& query, size_t limit) const {
    auto query_keywords = extract_keywords(query);

    // Score all episodes
    std::vector<std::pair<float, EpisodeId>> scores;
    for (const auto& entry : index_) {
        float score = keyword_score(entry.keywords, query_keywords);
        if (score > 0) {
            scores.emplace_back(score, entry.id);
        }
    }

    // Sort by score descending
    std::sort(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // Load top results
    std::vector<Episode> results;
    for (size_t i = 0; i < std::min(limit, scores.size()); ++i) {
        auto ep = get(scores[i].second);
        if (ep.is_ok()) {
            results.push_back(std::move(ep).value());
        }
    }

    return results;
}

std::vector<Episode> EpisodicMemory::search_by_category(const std::string& category, size_t limit) const {
    std::vector<Episode> results;

    for (const auto& entry : index_) {
        if (entry.category == category) {
            auto ep = get(entry.id);
            if (ep.is_ok()) {
                results.push_back(std::move(ep).value());
                if (results.size() >= limit) break;
            }
        }
    }

    return results;
}

std::vector<Episode> EpisodicMemory::get_recent(size_t limit) const {
    // Index is sorted by timestamp (newest first after sorting)
    auto sorted_index = index_;
    std::sort(sorted_index.begin(), sorted_index.end(),
        [](const auto& a, const auto& b) { return a.timestamp > b.timestamp; });

    std::vector<Episode> results;
    for (size_t i = 0; i < std::min(limit, sorted_index.size()); ++i) {
        auto ep = get(sorted_index[i].id);
        if (ep.is_ok()) {
            results.push_back(std::move(ep).value());
        }
    }

    return results;
}

std::vector<Episode> EpisodicMemory::get_successful(size_t limit) const {
    std::vector<Episode> results;

    for (const auto& entry : index_) {
        if (entry.success) {
            auto ep = get(entry.id);
            if (ep.is_ok()) {
                results.push_back(std::move(ep).value());
                if (results.size() >= limit) break;
            }
        }
    }

    return results;
}

size_t EpisodicMemory::count() const {
    return index_.size();
}

size_t EpisodicMemory::count_successful() const {
    return std::count_if(index_.begin(), index_.end(),
        [](const auto& entry) { return entry.success; });
}

std::vector<Episode> EpisodicMemory::all_episodes() const {
    std::vector<Episode> results;
    results.reserve(index_.size());

    for (const auto& entry : index_) {
        auto ep = get(entry.id);
        if (ep.is_ok()) {
            results.push_back(std::move(ep).value());
        }
    }

    return results;
}

void EpisodicMemory::update_index(const Episode& episode) {
    // Remove existing entry with same ID
    index_.erase(
        std::remove_if(index_.begin(), index_.end(),
            [&](const auto& entry) { return entry.id == episode.id; }),
        index_.end()
    );

    // Add new entry
    EpisodeIndexEntry entry;
    entry.id = episode.id;
    entry.keywords = episode.keywords.empty() ?
        extract_keywords(episode.task_description) : episode.keywords;
    entry.category = episode.task_category;
    entry.success = episode.outcome.success;
    entry.timestamp = episode.timestamp;
    entry.turns = episode.outcome.turns_taken;

    index_.push_back(entry);
}

Result<void, Error> EpisodicMemory::load_index() {
    try {
        if (!fs::exists(index_path_)) {
            return Result<void, Error>::ok();
        }

        std::ifstream file(index_path_);
        if (!file) {
            return Result<void, Error>::err(
                ErrorCode::FileReadFailed,
                "Failed to open index file",
                index_path_.string()
            );
        }

        Json j = Json::parse(file);
        index_.clear();

        for (const auto& item : j) {
            index_.push_back(EpisodeIndexEntry::from_json(item));
        }

        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        // Index corruption is recoverable - just start fresh
        index_.clear();
        return Result<void, Error>::ok();
    }
}

Result<void, Error> EpisodicMemory::save_index() const {
    try {
        std::ofstream file(index_path_);
        if (!file) {
            return Result<void, Error>::err(
                ErrorCode::FileWriteFailed,
                "Failed to open index file for writing",
                index_path_.string()
            );
        }

        Json j = Json::array();
        for (const auto& entry : index_) {
            j.push_back(entry.to_json());
        }

        file << j.dump(2);
        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what(),
            index_path_.string()
        );
    }
}

}  // namespace gpagent::memory
