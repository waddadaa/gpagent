#pragma once

#include "gpagent/core/config.hpp"
#include "gpagent/core/result.hpp"
#include "gpagent/core/types.hpp"

#include "session_state.hpp"
#include "thread_memory.hpp"
#include "episodic_memory.hpp"
#include "checkpointer.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace gpagent::memory {

using namespace gpagent::core;
namespace fs = std::filesystem;

// Cross-thread memory - facts that persist across sessions
class CrossThreadMemory {
public:
    explicit CrossThreadMemory(const fs::path& storage_path);

    // Store a fact in a namespace
    void store(const std::string& ns, const std::string& key, const Json& value);

    // Retrieve a fact
    std::optional<Json> retrieve(const std::string& ns, const std::string& key) const;

    // List all keys in a namespace
    std::vector<std::string> list_keys(const std::string& ns) const;

    // Delete a fact
    void remove(const std::string& ns, const std::string& key);

    // Persistence
    Result<void, Error> save() const;
    Result<void, Error> load();

private:
    fs::path storage_path_;
    std::map<std::string, std::map<std::string, Json>> data_;
};

// Main memory manager - coordinates all memory subsystems
class MemoryManager {
public:
    explicit MemoryManager(const MemoryConfig& config);

    // Session management
    Result<void, Error> start_session(const SessionId& id);
    Result<void, Error> resume_session(const SessionId& id);
    Result<void, Error> end_session();
    bool has_active_session() const;
    const SessionId& current_session_id() const;

    // List all available sessions with metadata
    struct SessionInfo {
        SessionId id;
        TimePoint created_at;
        TimePoint updated_at;
        std::string preview;  // First message or description
    };
    std::vector<SessionInfo> list_sessions() const;

    // Session state access
    SessionState& session_state();
    const SessionState& session_state() const;

    // Thread memory access
    ThreadMemory& thread_memory();
    const ThreadMemory& thread_memory() const;

    // Compressed history
    CompressedHistory& compressed_history();
    const CompressedHistory& compressed_history() const;

    // Message operations
    void append_message(const Message& message);
    std::vector<Message> get_recent_turns(int n) const;
    std::string get_compressed_history() const;

    // Cross-thread memory
    void store_fact(const std::string& ns, const std::string& key, const Json& value);
    std::optional<Json> retrieve_fact(const std::string& ns, const std::string& key) const;

    // Episodic memory
    Result<void, Error> store_episode(const Episode& episode);
    std::vector<Episode> retrieve_episodes(const std::string& query, int limit = 5) const;
    size_t episode_count() const;
    size_t successful_episode_count() const;

    // Checkpointing
    Result<CheckpointId, Error> create_checkpoint(const std::string& description = "");
    Result<void, Error> restore_checkpoint(const CheckpointId& id);
    std::vector<CheckpointInfo> list_checkpoints() const;

    // User/Project memory (markdown files)
    std::string get_user_memory() const;
    std::string get_project_memory() const;
    void update_user_memory(const std::string& content);
    void update_project_memory(const std::string& content);

    // Persistence
    Result<void, Error> save_all();
    Result<void, Error> load_all();

    // Initialize the manager (creates default session if needed)
    Result<void, Error> initialize();

    // Add a message to thread memory
    void add_message(const Message& message);

    // Direct access to episodic memory
    EpisodicMemory& episodic_memory() { return *episodic_; }
    const EpisodicMemory& episodic_memory() const { return *episodic_; }

    // Direct access to checkpointer
    Checkpointer& checkpointer() { return *checkpointer_; }
    const Checkpointer& checkpointer() const { return *checkpointer_; }

    // Get config
    const MemoryConfig& config() const { return config_; }

private:
    MemoryConfig config_;
    fs::path storage_path_;

    // Current session
    std::optional<SessionId> current_session_id_;
    std::optional<SessionState> session_state_;
    std::optional<ThreadMemory> thread_memory_;
    std::optional<CompressedHistory> compressed_history_;

    // Persistent components
    std::unique_ptr<CrossThreadMemory> cross_thread_;
    std::unique_ptr<EpisodicMemory> episodic_;
    std::unique_ptr<Checkpointer> checkpointer_;

    // Paths
    fs::path session_path(const SessionId& id) const;
    fs::path user_memory_path() const;
    fs::path project_memory_path() const;

    // Helper to ensure directories exist
    void ensure_directories();
};

}  // namespace gpagent::memory
