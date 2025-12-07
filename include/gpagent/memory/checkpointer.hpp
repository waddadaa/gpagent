#pragma once

#include "gpagent/core/types.hpp"
#include "gpagent/core/result.hpp"
#include "session_state.hpp"
#include "thread_memory.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gpagent::memory {

using namespace gpagent::core;
namespace fs = std::filesystem;

// Checkpoint metadata
struct CheckpointInfo {
    CheckpointId id;
    SessionId session_id;
    ThreadId thread_id;
    TimePoint timestamp;
    std::optional<CheckpointId> parent_id;
    std::string description;
    std::string trigger;  // manual, auto, before_refactor, etc.
    int conversation_turn;

    Json to_json() const;
    static CheckpointInfo from_json(const Json& j);
};

// Full checkpoint data
struct Checkpoint {
    CheckpointInfo info;
    SessionState session_state;
    ThreadMemory thread_memory;
    CompressedHistory compressed_history;
    Json custom_state;

    Json to_json() const;
    static Checkpoint from_json(const Json& j);
};

// Checkpointer - manages state checkpoints for branching/restoring
class Checkpointer {
public:
    explicit Checkpointer(const fs::path& storage_path);

    // Create a new checkpoint
    Result<CheckpointId, Error> create(
        const SessionState& session,
        const ThreadMemory& thread,
        const CompressedHistory& history,
        const std::string& description = "",
        const std::string& trigger = "manual"
    );

    // Create checkpoint with parent reference
    Result<CheckpointId, Error> create(
        const SessionState& session,
        const ThreadMemory& thread,
        const CompressedHistory& history,
        const CheckpointId& parent_id,
        const std::string& description,
        const std::string& trigger
    );

    // Restore from checkpoint
    Result<Checkpoint, Error> restore(const CheckpointId& id) const;

    // Get checkpoint info without full data
    Result<CheckpointInfo, Error> get_info(const CheckpointId& id) const;

    // List all checkpoints for a session
    std::vector<CheckpointInfo> list(const SessionId& session_id) const;

    // List all checkpoints (across all sessions)
    std::vector<CheckpointInfo> list_all() const;

    // Delete a checkpoint
    Result<void, Error> remove(const CheckpointId& id);

    // Get most recent checkpoint for session
    std::optional<CheckpointInfo> get_latest(const SessionId& session_id) const;

    // Check if checkpoint exists
    bool exists(const CheckpointId& id) const;

private:
    fs::path storage_path_;

    fs::path checkpoint_path(const CheckpointId& id) const;
    fs::path info_path(const CheckpointId& id) const;

    Result<void, Error> load_index();
    Result<void, Error> save_index() const;

    std::vector<CheckpointInfo> index_;
};

}  // namespace gpagent::memory
