#include "gpagent/trm/trm_model.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace gpagent::trm {

TRMModel::TRMModel(const TRMConfig& config)
    : config_(config)
{
    // Start in ColdStart mode for unsupervised learning
    // This allows fallback predictions while collecting training data
    status_ = TRMStatus::ColdStart;
}

bool TRMModel::can_start_training(size_t episode_count) const {
    return episode_count >= static_cast<size_t>(config_.min_episodes_before_training);
}

std::optional<TRMPrediction> TRMModel::predict(
    const std::string& task_context,
    const std::vector<std::string>& available_tools,
    const std::vector<memory::EpisodeAction>& history) {

    // In ColdStart mode, use fallback with history boosting
    if (status_ == TRMStatus::ColdStart) {
        auto prediction = fallback_predict(task_context, available_tools);

        // Apply history boosting for unsupervised learning improvement
        if (!history.empty()) {
            std::unordered_map<std::string, int> history_scores;
            int recency = static_cast<int>(history.size());

            for (const auto& action : history) {
                history_scores[action.tool] += recency;
                --recency;
            }

            for (auto& [tool, score] : prediction.ranked_tools) {
                auto it = history_scores.find(tool);
                if (it != history_scores.end()) {
                    float history_boost = static_cast<float>(it->second) /
                                          static_cast<float>(history.size()) * 0.15f;
                    score = std::min(1.0f, score + history_boost);
                }
            }

            std::sort(prediction.ranked_tools.begin(), prediction.ranked_tools.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });

            if (!prediction.ranked_tools.empty()) {
                prediction.recommended_tool = prediction.ranked_tools[0].first;
                prediction.confidence = prediction.ranked_tools[0].second;
            }
        }

        return prediction;
    }

    // Not ready for predictions
    if (status_ != TRMStatus::Ready) {
        return std::nullopt;
    }

    // NOTE: In a full implementation, this would run inference through the
    // trained neural network. For now, we use a more sophisticated heuristic
    // that can be replaced with actual model inference.

    // This placeholder uses:
    // 1. Keyword matching for context
    // 2. History-based frequency analysis
    // 3. Tool co-occurrence patterns

    TRMPrediction prediction;
    prediction.ranked_tools = keyword_match(task_context, available_tools);

    // Boost tools that were recently successful in history
    if (!history.empty()) {
        std::unordered_map<std::string, int> history_scores;
        int recency = static_cast<int>(history.size());

        for (const auto& action : history) {
            // More recent actions get higher scores
            history_scores[action.tool] += recency;
            --recency;
        }

        // Apply history boost
        for (auto& [tool, score] : prediction.ranked_tools) {
            auto it = history_scores.find(tool);
            if (it != history_scores.end()) {
                // Boost by history score, normalized
                float history_boost = static_cast<float>(it->second) /
                                      static_cast<float>(history.size()) * 0.2f;
                score = std::min(1.0f, score + history_boost);
            }
        }

        // Re-sort after boosting
        std::sort(prediction.ranked_tools.begin(), prediction.ranked_tools.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
    }

    if (!prediction.ranked_tools.empty()) {
        prediction.recommended_tool = prediction.ranked_tools[0].first;
        prediction.confidence = prediction.ranked_tools[0].second;
    }

    return prediction;
}

TRMPrediction TRMModel::fallback_predict(
    const std::string& task_context,
    const std::vector<std::string>& available_tools) {

    TRMPrediction prediction;
    prediction.ranked_tools = keyword_match(task_context, available_tools);

    if (!prediction.ranked_tools.empty()) {
        prediction.recommended_tool = prediction.ranked_tools[0].first;
        prediction.confidence = prediction.ranked_tools[0].second * 0.5f;  // Lower confidence for fallback
    } else if (!available_tools.empty()) {
        // If no keyword matches, return first tool with very low confidence
        prediction.recommended_tool = available_tools[0];
        prediction.confidence = 0.1f;
        for (const auto& tool : available_tools) {
            prediction.ranked_tools.emplace_back(tool, 0.1f);
        }
    }

    return prediction;
}

Result<void, Error> TRMModel::load(const fs::path& path) {
    if (!fs::exists(path)) {
        return Result<void, Error>::err(
            ErrorCode::FileNotFound,
            "Model file not found: " + path.string()
        );
    }

    // NOTE: In a full implementation, this would deserialize the model weights
    // from the specified path. The format could be ONNX, SafeTensors, or custom.

    // For now, we just check the file exists and mark as ready
    // In production, this would:
    // 1. Load model architecture from config
    // 2. Deserialize weights from file
    // 3. Initialize inference runtime (CPU/GPU)
    // 4. Run validation inference

    status_ = TRMStatus::Ready;
    return Result<void, Error>::ok();
}

Result<void, Error> TRMModel::save(const fs::path& path) const {
    if (status_ != TRMStatus::Ready && status_ != TRMStatus::Training) {
        return Result<void, Error>::err(
            ErrorCode::InvalidState,
            "Cannot save model that is not initialized"
        );
    }

    // Ensure parent directory exists
    auto parent = path.parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            return Result<void, Error>::err(
                ErrorCode::FileWriteFailed,
                "Failed to create model directory: " + ec.message()
            );
        }
    }

    // NOTE: In a full implementation, this would serialize the model weights
    // For now, we create a placeholder file

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            "Failed to open model file for writing: " + path.string()
        );
    }

    // Write a simple header for now
    const char* header = "GPAGENT_TRM_V1";
    ofs.write(header, 14);

    return Result<void, Error>::ok();
}

std::vector<std::pair<ToolId, float>> TRMModel::keyword_match(
    const std::string& query,
    const std::vector<std::string>& tools) {

    // Convert query to lowercase for matching
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Extract words from query
    std::unordered_set<std::string> query_words;
    std::istringstream iss(lower_query);
    std::string word;
    while (iss >> word) {
        // Remove punctuation
        word.erase(std::remove_if(word.begin(), word.end(),
                                   [](unsigned char c) { return std::ispunct(c); }),
                   word.end());
        if (word.length() > 2) {  // Skip very short words
            query_words.insert(word);
        }
    }

    // Tool-specific keywords for matching
    static const std::unordered_map<std::string, std::vector<std::string>> tool_keywords = {
        {"file_read", {"read", "file", "content", "show", "view", "cat", "look", "see", "check", "open", "text"}},
        {"file_write", {"write", "create", "save", "new", "file", "output", "generate"}},
        {"file_edit", {"edit", "modify", "change", "update", "fix", "replace", "refactor"}},
        {"bash", {"run", "execute", "command", "shell", "terminal", "script", "install", "build", "compile", "test"}},
        {"grep", {"search", "find", "grep", "look", "locate", "pattern", "match", "where", "code"}},
        {"glob", {"files", "list", "find", "pattern", "directory", "folder", "ls"}},
        {"image_read", {"image", "picture", "photo", "screenshot", "png", "jpg", "jpeg", "gif", "see", "look", "show", "visual"}},
        {"web_search", {"search", "web", "internet", "google", "online", "find", "lookup", "query", "information"}},
        {"web_fetch", {"fetch", "url", "website", "page", "download", "http", "link", "browse", "visit"}},
    };

    std::vector<std::pair<ToolId, float>> scores;

    for (const auto& tool : tools) {
        float score = 0.0f;

        // Check if tool name appears in query
        std::string lower_tool = tool;
        std::transform(lower_tool.begin(), lower_tool.end(), lower_tool.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lower_query.find(lower_tool) != std::string::npos) {
            score += 0.5f;
        }

        // Check keyword matches
        auto it = tool_keywords.find(tool);
        if (it != tool_keywords.end()) {
            int matches = 0;
            for (const auto& keyword : it->second) {
                if (query_words.count(keyword) > 0) {
                    ++matches;
                }
            }
            // Normalize by number of keywords
            if (!it->second.empty()) {
                score += static_cast<float>(matches) / static_cast<float>(it->second.size()) * 0.5f;
            }
        }

        scores.emplace_back(tool, score);
    }

    // Sort by score descending
    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    return scores;
}

}  // namespace gpagent::trm
