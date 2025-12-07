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

// Current task being worked on
struct CurrentTask {
    std::string description;
    std::string status;  // pending, in_progress, completed
    TimePoint started_at;
    std::optional<TimePoint> completed_at;

    Json to_json() const;
    static CurrentTask from_json(const Json& j);
};

// Scratchpad for temporary working data
struct Scratchpad {
    std::vector<std::string> files_modified;
    std::vector<std::string> pending_actions;
    Json custom_data;

    Json to_json() const;
    static Scratchpad from_json(const Json& j);
};

// Tool state from last execution
struct ToolState {
    std::string last_tool;
    std::string last_result;  // success, error
    std::optional<std::string> last_error_message;
    TimePoint last_execution;

    Json to_json() const;
    static ToolState from_json(const Json& j);
};

// Session state - short-term memory for a single session
class SessionState {
public:
    SessionState();
    explicit SessionState(const SessionId& id);

    // Accessors
    const SessionId& id() const { return session_id_; }
    int conversation_turn() const { return conversation_turn_; }
    TimePoint created_at() const { return created_at_; }
    TimePoint updated_at() const { return updated_at_; }

    // Task management
    const std::optional<CurrentTask>& current_task() const { return current_task_; }
    void set_current_task(const std::string& description);
    void update_task_status(const std::string& status);
    void complete_task();
    void clear_task();

    // Scratchpad
    Scratchpad& scratchpad() { return scratchpad_; }
    const Scratchpad& scratchpad() const { return scratchpad_; }
    void add_modified_file(const std::string& path);
    void add_pending_action(const std::string& action);
    void clear_pending_actions();

    // Tool state
    const ToolState& tool_state() const { return tool_state_; }
    void record_tool_execution(const std::string& tool, bool success,
                                const std::optional<std::string>& error = std::nullopt);

    // Turn management
    void increment_turn();
    void touch();  // Update updated_at timestamp

    // Serialization
    Json to_json() const;
    static SessionState from_json(const Json& j);

    // File I/O
    Result<void, Error> save(const fs::path& path) const;
    static Result<SessionState, Error> load(const fs::path& path);

private:
    SessionId session_id_;
    TimePoint created_at_;
    TimePoint updated_at_;
    int conversation_turn_ = 0;

    std::optional<CurrentTask> current_task_;
    Scratchpad scratchpad_;
    ToolState tool_state_;
};

}  // namespace gpagent::memory
