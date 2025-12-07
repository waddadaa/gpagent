#include "gpagent/memory/session_state.hpp"
#include "gpagent/core/uuid.hpp"

#include <fstream>

namespace gpagent::memory {

// CurrentTask
Json CurrentTask::to_json() const {
    Json j{
        {"description", description},
        {"status", status},
        {"started_at", std::chrono::duration_cast<std::chrono::seconds>(
            started_at.time_since_epoch()).count()}
    };
    if (completed_at) {
        j["completed_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            completed_at->time_since_epoch()).count();
    }
    return j;
}

CurrentTask CurrentTask::from_json(const Json& j) {
    CurrentTask task;
    task.description = j.value("description", "");
    task.status = j.value("status", "pending");
    if (j.contains("started_at")) {
        task.started_at = TimePoint{std::chrono::seconds{j["started_at"].get<int64_t>()}};
    }
    if (j.contains("completed_at")) {
        task.completed_at = TimePoint{std::chrono::seconds{j["completed_at"].get<int64_t>()}};
    }
    return task;
}

// Scratchpad
Json Scratchpad::to_json() const {
    return Json{
        {"files_modified", files_modified},
        {"pending_actions", pending_actions},
        {"custom_data", custom_data}
    };
}

Scratchpad Scratchpad::from_json(const Json& j) {
    Scratchpad sp;
    if (j.contains("files_modified")) {
        sp.files_modified = j["files_modified"].get<std::vector<std::string>>();
    }
    if (j.contains("pending_actions")) {
        sp.pending_actions = j["pending_actions"].get<std::vector<std::string>>();
    }
    if (j.contains("custom_data")) {
        sp.custom_data = j["custom_data"];
    }
    return sp;
}

// ToolState
Json ToolState::to_json() const {
    Json j{
        {"last_tool", last_tool},
        {"last_result", last_result},
        {"last_execution", std::chrono::duration_cast<std::chrono::seconds>(
            last_execution.time_since_epoch()).count()}
    };
    if (last_error_message) {
        j["last_error_message"] = *last_error_message;
    }
    return j;
}

ToolState ToolState::from_json(const Json& j) {
    ToolState ts;
    ts.last_tool = j.value("last_tool", "");
    ts.last_result = j.value("last_result", "");
    if (j.contains("last_execution")) {
        ts.last_execution = TimePoint{std::chrono::seconds{j["last_execution"].get<int64_t>()}};
    }
    if (j.contains("last_error_message")) {
        ts.last_error_message = j["last_error_message"].get<std::string>();
    }
    return ts;
}

// SessionState
SessionState::SessionState()
    : session_id_(generate_session_id())
    , created_at_(Clock::now())
    , updated_at_(Clock::now())
{
}

SessionState::SessionState(const SessionId& id)
    : session_id_(id)
    , created_at_(Clock::now())
    , updated_at_(Clock::now())
{
}

void SessionState::set_current_task(const std::string& description) {
    current_task_ = CurrentTask{
        .description = description,
        .status = "in_progress",
        .started_at = Clock::now(),
        .completed_at = std::nullopt
    };
    touch();
}

void SessionState::update_task_status(const std::string& status) {
    if (current_task_) {
        current_task_->status = status;
        touch();
    }
}

void SessionState::complete_task() {
    if (current_task_) {
        current_task_->status = "completed";
        current_task_->completed_at = Clock::now();
        touch();
    }
}

void SessionState::clear_task() {
    current_task_ = std::nullopt;
    touch();
}

void SessionState::add_modified_file(const std::string& path) {
    // Avoid duplicates
    auto& files = scratchpad_.files_modified;
    if (std::find(files.begin(), files.end(), path) == files.end()) {
        files.push_back(path);
    }
    touch();
}

void SessionState::add_pending_action(const std::string& action) {
    scratchpad_.pending_actions.push_back(action);
    touch();
}

void SessionState::clear_pending_actions() {
    scratchpad_.pending_actions.clear();
    touch();
}

void SessionState::record_tool_execution(const std::string& tool, bool success,
                                          const std::optional<std::string>& error) {
    tool_state_.last_tool = tool;
    tool_state_.last_result = success ? "success" : "error";
    tool_state_.last_error_message = error;
    tool_state_.last_execution = Clock::now();
    touch();
}

void SessionState::increment_turn() {
    ++conversation_turn_;
    touch();
}

void SessionState::touch() {
    updated_at_ = Clock::now();
}

Json SessionState::to_json() const {
    Json j{
        {"session_id", session_id_},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            created_at_.time_since_epoch()).count()},
        {"updated_at", std::chrono::duration_cast<std::chrono::seconds>(
            updated_at_.time_since_epoch()).count()},
        {"conversation_turn", conversation_turn_},
        {"scratchpad", scratchpad_.to_json()},
        {"tool_state", tool_state_.to_json()}
    };

    if (current_task_) {
        j["current_task"] = current_task_->to_json();
    }

    return j;
}

SessionState SessionState::from_json(const Json& j) {
    SessionState state;
    state.session_id_ = j.value("session_id", "");
    state.conversation_turn_ = j.value("conversation_turn", 0);

    if (j.contains("created_at")) {
        state.created_at_ = TimePoint{std::chrono::seconds{j["created_at"].get<int64_t>()}};
    }
    if (j.contains("updated_at")) {
        state.updated_at_ = TimePoint{std::chrono::seconds{j["updated_at"].get<int64_t>()}};
    }
    if (j.contains("current_task")) {
        state.current_task_ = CurrentTask::from_json(j["current_task"]);
    }
    if (j.contains("scratchpad")) {
        state.scratchpad_ = Scratchpad::from_json(j["scratchpad"]);
    }
    if (j.contains("tool_state")) {
        state.tool_state_ = ToolState::from_json(j["tool_state"]);
    }

    return state;
}

Result<void, Error> SessionState::save(const fs::path& path) const {
    try {
        // Ensure parent directory exists
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
        }

        std::ofstream file(path);
        if (!file) {
            return Result<void, Error>::err(
                ErrorCode::FileWriteFailed,
                "Failed to open file for writing",
                path.string()
            );
        }

        file << to_json().dump(2);
        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what(),
            path.string()
        );
    }
}

Result<SessionState, Error> SessionState::load(const fs::path& path) {
    try {
        if (!fs::exists(path)) {
            return Result<SessionState, Error>::err(
                ErrorCode::FileNotFound,
                "Session state file not found",
                path.string()
            );
        }

        std::ifstream file(path);
        if (!file) {
            return Result<SessionState, Error>::err(
                ErrorCode::FileReadFailed,
                "Failed to open file for reading",
                path.string()
            );
        }

        Json j = Json::parse(file);
        return Result<SessionState, Error>::ok(SessionState::from_json(j));

    } catch (const Json::exception& e) {
        return Result<SessionState, Error>::err(
            ErrorCode::MemoryCorrupted,
            std::string("JSON parse error: ") + e.what(),
            path.string()
        );
    } catch (const std::exception& e) {
        return Result<SessionState, Error>::err(
            ErrorCode::FileReadFailed,
            e.what(),
            path.string()
        );
    }
}

}  // namespace gpagent::memory
