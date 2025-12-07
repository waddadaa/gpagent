#include "gpagent/memory/checkpointer.hpp"
#include "gpagent/core/uuid.hpp"

#include <algorithm>
#include <fstream>

namespace gpagent::memory {

// CheckpointInfo
Json CheckpointInfo::to_json() const {
    Json j{
        {"id", id},
        {"session_id", session_id},
        {"thread_id", thread_id},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            timestamp.time_since_epoch()).count()},
        {"description", description},
        {"trigger", trigger},
        {"conversation_turn", conversation_turn}
    };

    if (parent_id) {
        j["parent_id"] = *parent_id;
    }

    return j;
}

CheckpointInfo CheckpointInfo::from_json(const Json& j) {
    CheckpointInfo info;
    info.id = j.value("id", "");
    info.session_id = j.value("session_id", "");
    info.thread_id = j.value("thread_id", "");
    info.description = j.value("description", "");
    info.trigger = j.value("trigger", "manual");
    info.conversation_turn = j.value("conversation_turn", 0);

    if (j.contains("timestamp")) {
        info.timestamp = TimePoint{std::chrono::seconds{j["timestamp"].get<int64_t>()}};
    }

    if (j.contains("parent_id")) {
        info.parent_id = j["parent_id"].get<std::string>();
    }

    return info;
}

// Checkpoint
Json Checkpoint::to_json() const {
    return Json{
        {"info", info.to_json()},
        {"session_state", session_state.to_json()},
        {"custom_state", custom_state}
        // thread_memory and compressed_history saved separately
    };
}

Checkpoint Checkpoint::from_json(const Json& j) {
    Checkpoint cp;
    if (j.contains("info")) {
        cp.info = CheckpointInfo::from_json(j["info"]);
    }
    if (j.contains("session_state")) {
        cp.session_state = SessionState::from_json(j["session_state"]);
    }
    if (j.contains("custom_state")) {
        cp.custom_state = j["custom_state"];
    }
    return cp;
}

// Checkpointer
Checkpointer::Checkpointer(const fs::path& storage_path)
    : storage_path_(storage_path)
{
    fs::create_directories(storage_path_);
    load_index();
}

fs::path Checkpointer::checkpoint_path(const CheckpointId& id) const {
    return storage_path_ / id;
}

fs::path Checkpointer::info_path(const CheckpointId& id) const {
    return checkpoint_path(id) / "info.json";
}

Result<CheckpointId, Error> Checkpointer::create(
    const SessionState& session,
    const ThreadMemory& thread,
    const CompressedHistory& history,
    const std::string& description,
    const std::string& trigger)
{
    return create(session, thread, history, "", description, trigger);
}

Result<CheckpointId, Error> Checkpointer::create(
    const SessionState& session,
    const ThreadMemory& thread,
    const CompressedHistory& history,
    const CheckpointId& parent_id,
    const std::string& description,
    const std::string& trigger)
{
    try {
        CheckpointId id = generate_checkpoint_id();
        fs::path cp_path = checkpoint_path(id);
        fs::create_directories(cp_path);

        // Create checkpoint info
        CheckpointInfo info;
        info.id = id;
        info.session_id = session.id();
        info.thread_id = thread.id();
        info.timestamp = Clock::now();
        info.parent_id = parent_id.empty() ? std::nullopt : std::make_optional(parent_id);
        info.description = description;
        info.trigger = trigger;
        info.conversation_turn = session.conversation_turn();

        // Save checkpoint info
        {
            std::ofstream file(info_path(id));
            if (!file) {
                return Result<CheckpointId, Error>::err(
                    ErrorCode::FileWriteFailed,
                    "Failed to save checkpoint info",
                    info_path(id).string()
                );
            }
            file << info.to_json().dump(2);
        }

        // Save session state
        auto session_result = session.save(cp_path / "session.json");
        if (session_result.is_err()) {
            return Result<CheckpointId, Error>::err(std::move(session_result).error());
        }

        // Save thread memory
        auto thread_result = thread.save(cp_path / "thread.jsonl");
        if (thread_result.is_err()) {
            return Result<CheckpointId, Error>::err(std::move(thread_result).error());
        }

        // Save compressed history
        auto history_result = history.save(cp_path / "history.json");
        if (history_result.is_err()) {
            return Result<CheckpointId, Error>::err(std::move(history_result).error());
        }

        // Update index
        index_.push_back(info);
        save_index();

        return Result<CheckpointId, Error>::ok(id);

    } catch (const std::exception& e) {
        return Result<CheckpointId, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what()
        );
    }
}

Result<Checkpoint, Error> Checkpointer::restore(const CheckpointId& id) const {
    try {
        fs::path cp_path = checkpoint_path(id);

        if (!fs::exists(cp_path)) {
            return Result<Checkpoint, Error>::err(
                ErrorCode::CheckpointNotFound,
                "Checkpoint not found",
                id
            );
        }

        Checkpoint cp;

        // Load info
        auto info_result = get_info(id);
        if (info_result.is_err()) {
            return Result<Checkpoint, Error>::err(std::move(info_result).error());
        }
        cp.info = std::move(info_result).value();

        // Load session state
        auto session_result = SessionState::load(cp_path / "session.json");
        if (session_result.is_err()) {
            return Result<Checkpoint, Error>::err(std::move(session_result).error());
        }
        cp.session_state = std::move(session_result).value();

        // Load thread memory
        auto thread_result = ThreadMemory::load(cp_path / "thread.jsonl");
        if (thread_result.is_err()) {
            return Result<Checkpoint, Error>::err(std::move(thread_result).error());
        }
        cp.thread_memory = std::move(thread_result).value();

        // Load compressed history
        auto history_result = CompressedHistory::load(cp_path / "history.json");
        if (history_result.is_err()) {
            return Result<Checkpoint, Error>::err(std::move(history_result).error());
        }
        cp.compressed_history = std::move(history_result).value();

        return Result<Checkpoint, Error>::ok(std::move(cp));

    } catch (const std::exception& e) {
        return Result<Checkpoint, Error>::err(
            ErrorCode::FileReadFailed,
            e.what(),
            id
        );
    }
}

Result<CheckpointInfo, Error> Checkpointer::get_info(const CheckpointId& id) const {
    try {
        fs::path path = info_path(id);

        if (!fs::exists(path)) {
            return Result<CheckpointInfo, Error>::err(
                ErrorCode::CheckpointNotFound,
                "Checkpoint info not found",
                id
            );
        }

        std::ifstream file(path);
        if (!file) {
            return Result<CheckpointInfo, Error>::err(
                ErrorCode::FileReadFailed,
                "Failed to open checkpoint info",
                path.string()
            );
        }

        Json j = Json::parse(file);
        return Result<CheckpointInfo, Error>::ok(CheckpointInfo::from_json(j));

    } catch (const std::exception& e) {
        return Result<CheckpointInfo, Error>::err(
            ErrorCode::FileReadFailed,
            e.what(),
            id
        );
    }
}

std::vector<CheckpointInfo> Checkpointer::list(const SessionId& session_id) const {
    std::vector<CheckpointInfo> result;

    for (const auto& info : index_) {
        if (info.session_id == session_id) {
            result.push_back(info);
        }
    }

    // Sort by timestamp descending
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) { return a.timestamp > b.timestamp; });

    return result;
}

std::vector<CheckpointInfo> Checkpointer::list_all() const {
    auto result = index_;

    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) { return a.timestamp > b.timestamp; });

    return result;
}

Result<void, Error> Checkpointer::remove(const CheckpointId& id) {
    try {
        fs::path cp_path = checkpoint_path(id);

        if (!fs::exists(cp_path)) {
            return Result<void, Error>::err(
                ErrorCode::CheckpointNotFound,
                "Checkpoint not found",
                id
            );
        }

        fs::remove_all(cp_path);

        // Update index
        index_.erase(
            std::remove_if(index_.begin(), index_.end(),
                [&](const auto& info) { return info.id == id; }),
            index_.end()
        );
        save_index();

        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what(),
            id
        );
    }
}

std::optional<CheckpointInfo> Checkpointer::get_latest(const SessionId& session_id) const {
    auto checkpoints = list(session_id);
    if (checkpoints.empty()) {
        return std::nullopt;
    }
    return checkpoints.front();  // Already sorted by timestamp desc
}

bool Checkpointer::exists(const CheckpointId& id) const {
    return fs::exists(checkpoint_path(id));
}

Result<void, Error> Checkpointer::load_index() {
    try {
        fs::path path = storage_path_ / "index.json";

        if (!fs::exists(path)) {
            return Result<void, Error>::ok();
        }

        std::ifstream file(path);
        if (!file) {
            return Result<void, Error>::ok();  // Recoverable
        }

        Json j = Json::parse(file);
        index_.clear();

        for (const auto& item : j) {
            index_.push_back(CheckpointInfo::from_json(item));
        }

        return Result<void, Error>::ok();

    } catch (const std::exception&) {
        index_.clear();
        return Result<void, Error>::ok();
    }
}

Result<void, Error> Checkpointer::save_index() const {
    try {
        fs::path path = storage_path_ / "index.json";

        std::ofstream file(path);
        if (!file) {
            return Result<void, Error>::err(
                ErrorCode::FileWriteFailed,
                "Failed to save checkpoint index",
                path.string()
            );
        }

        Json j = Json::array();
        for (const auto& info : index_) {
            j.push_back(info.to_json());
        }

        file << j.dump(2);
        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what()
        );
    }
}

}  // namespace gpagent::memory
