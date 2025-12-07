#include "gpagent/memory/memory_manager.hpp"
#include "gpagent/core/types.hpp"
#include "gpagent/core/uuid.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace gpagent::memory {

// CrossThreadMemory
CrossThreadMemory::CrossThreadMemory(const fs::path& storage_path)
    : storage_path_(storage_path)
{
    fs::create_directories(storage_path_);
    load();
}

void CrossThreadMemory::store(const std::string& ns, const std::string& key, const Json& value) {
    data_[ns][key] = value;
}

std::optional<Json> CrossThreadMemory::retrieve(const std::string& ns, const std::string& key) const {
    auto ns_it = data_.find(ns);
    if (ns_it == data_.end()) {
        return std::nullopt;
    }

    auto key_it = ns_it->second.find(key);
    if (key_it == ns_it->second.end()) {
        return std::nullopt;
    }

    return key_it->second;
}

std::vector<std::string> CrossThreadMemory::list_keys(const std::string& ns) const {
    std::vector<std::string> keys;

    auto ns_it = data_.find(ns);
    if (ns_it != data_.end()) {
        for (const auto& [key, _] : ns_it->second) {
            keys.push_back(key);
        }
    }

    return keys;
}

void CrossThreadMemory::remove(const std::string& ns, const std::string& key) {
    auto ns_it = data_.find(ns);
    if (ns_it != data_.end()) {
        ns_it->second.erase(key);
    }
}

Result<void, Error> CrossThreadMemory::save() const {
    try {
        fs::path path = storage_path_ / "cross_thread.json";

        std::ofstream file(path);
        if (!file) {
            return Result<void, Error>::err(
                ErrorCode::FileWriteFailed,
                "Failed to save cross-thread memory",
                path.string()
            );
        }

        Json j = Json::object();
        for (const auto& [ns, entries] : data_) {
            j[ns] = entries;
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

Result<void, Error> CrossThreadMemory::load() {
    try {
        fs::path path = storage_path_ / "cross_thread.json";

        if (!fs::exists(path)) {
            return Result<void, Error>::ok();
        }

        std::ifstream file(path);
        if (!file) {
            return Result<void, Error>::ok();
        }

        Json j = Json::parse(file);
        data_.clear();

        for (auto& [ns, entries] : j.items()) {
            for (auto& [key, value] : entries.items()) {
                data_[ns][key] = value;
            }
        }

        return Result<void, Error>::ok();

    } catch (const std::exception&) {
        data_.clear();
        return Result<void, Error>::ok();
    }
}

// MemoryManager
MemoryManager::MemoryManager(const MemoryConfig& config)
    : config_(config)
    , storage_path_(expand_path(config.storage_path))
{
    ensure_directories();

    cross_thread_ = std::make_unique<CrossThreadMemory>(storage_path_ / "cross_thread");
    episodic_ = std::make_unique<EpisodicMemory>(storage_path_ / "episodic");
    checkpointer_ = std::make_unique<Checkpointer>(storage_path_ / "checkpoints");
}

void MemoryManager::ensure_directories() {
    fs::create_directories(storage_path_);
    fs::create_directories(storage_path_ / "sessions");
    fs::create_directories(storage_path_ / "cross_thread");
    fs::create_directories(storage_path_ / "episodic");
    fs::create_directories(storage_path_ / "checkpoints");
}

fs::path MemoryManager::session_path(const SessionId& id) const {
    return storage_path_ / "sessions" / id;
}

fs::path MemoryManager::user_memory_path() const {
    return storage_path_ / "user_memory.md";
}

fs::path MemoryManager::project_memory_path() const {
    return storage_path_ / "project_memory.md";
}

Result<void, Error> MemoryManager::start_session(const SessionId& id) {
    current_session_id_ = id;
    session_state_.emplace(id);
    thread_memory_.emplace(generate_thread_id());
    compressed_history_.emplace();

    // Create session directory
    fs::create_directories(session_path(id));

    return Result<void, Error>::ok();
}

Result<void, Error> MemoryManager::resume_session(const SessionId& id) {
    fs::path sess_path = session_path(id);

    if (!fs::exists(sess_path)) {
        return Result<void, Error>::err(
            ErrorCode::SessionNotFound,
            "Session not found",
            id
        );
    }

    // Load session state
    auto state_result = SessionState::load(sess_path / "state.json");
    if (state_result.is_err()) {
        return Result<void, Error>::err(std::move(state_result).error());
    }
    session_state_ = std::move(state_result).value();

    // Load thread memory
    auto thread_result = ThreadMemory::load(sess_path / "thread.jsonl");
    if (thread_result.is_ok()) {
        thread_memory_ = std::move(thread_result).value();
    } else {
        thread_memory_.emplace(generate_thread_id());
    }

    // Load compressed history
    auto history_result = CompressedHistory::load(sess_path / "history.json");
    if (history_result.is_ok()) {
        compressed_history_ = std::move(history_result).value();
    } else {
        compressed_history_.emplace();
    }

    current_session_id_ = id;
    return Result<void, Error>::ok();
}

Result<void, Error> MemoryManager::end_session() {
    if (!current_session_id_) {
        return Result<void, Error>::ok();
    }

    // Save everything
    auto save_result = save_all();

    current_session_id_ = std::nullopt;
    session_state_ = std::nullopt;
    thread_memory_ = std::nullopt;
    compressed_history_ = std::nullopt;

    return save_result;
}

bool MemoryManager::has_active_session() const {
    return current_session_id_.has_value();
}

const SessionId& MemoryManager::current_session_id() const {
    static const SessionId empty;
    return current_session_id_.value_or(empty);
}

std::vector<MemoryManager::SessionInfo> MemoryManager::list_sessions() const {
    std::vector<SessionInfo> sessions;

    fs::path sessions_dir = storage_path_ / "sessions";
    if (!fs::exists(sessions_dir)) {
        return sessions;
    }

    for (const auto& entry : fs::directory_iterator(sessions_dir)) {
        if (!entry.is_directory()) continue;

        SessionInfo info;
        info.id = entry.path().filename().string();

        // Try to load state.json for metadata
        fs::path state_path = entry.path() / "state.json";
        if (fs::exists(state_path)) {
            auto state_result = SessionState::load(state_path);
            if (state_result.is_ok()) {
                auto& state = state_result.value();
                info.created_at = state.created_at();
                info.updated_at = state.updated_at();
            }
        }

        // Try to load thread.jsonl for first message preview
        fs::path thread_path = entry.path() / "thread.jsonl";
        if (fs::exists(thread_path)) {
            auto thread_result = ThreadMemory::load(thread_path);
            if (thread_result.is_ok()) {
                auto& thread = thread_result.value();
                auto messages = thread.messages();
                for (const auto& msg : messages) {
                    if (msg.role == core::Role::User && !msg.content.empty()) {
                        // Truncate preview to 50 chars
                        info.preview = msg.content.substr(0, std::min(size_t(50), msg.content.size()));
                        if (msg.content.size() > 50) {
                            info.preview += "...";
                        }
                        break;
                    }
                }
            }
        }

        sessions.push_back(std::move(info));
    }

    // Sort by updated_at descending (most recent first)
    std::sort(sessions.begin(), sessions.end(),
              [](const SessionInfo& a, const SessionInfo& b) {
                  return a.updated_at > b.updated_at;
              });

    return sessions;
}

SessionState& MemoryManager::session_state() {
    if (!session_state_) {
        throw std::runtime_error("No active session");
    }
    return *session_state_;
}

const SessionState& MemoryManager::session_state() const {
    if (!session_state_) {
        throw std::runtime_error("No active session");
    }
    return *session_state_;
}

ThreadMemory& MemoryManager::thread_memory() {
    if (!thread_memory_) {
        throw std::runtime_error("No active session");
    }
    return *thread_memory_;
}

const ThreadMemory& MemoryManager::thread_memory() const {
    if (!thread_memory_) {
        throw std::runtime_error("No active session");
    }
    return *thread_memory_;
}

CompressedHistory& MemoryManager::compressed_history() {
    if (!compressed_history_) {
        throw std::runtime_error("No active session");
    }
    return *compressed_history_;
}

const CompressedHistory& MemoryManager::compressed_history() const {
    if (!compressed_history_) {
        throw std::runtime_error("No active session");
    }
    return *compressed_history_;
}

void MemoryManager::append_message(const Message& message) {
    if (!thread_memory_) return;

    thread_memory_->append(message);

    if (session_state_) {
        session_state_->increment_turn();

        // Auto-checkpoint if enabled
        if (config_.auto_checkpoint &&
            session_state_->conversation_turn() % config_.checkpoint_interval == 0) {
            create_checkpoint("auto");
        }
    }
}

std::vector<Message> MemoryManager::get_recent_turns(int n) const {
    if (!thread_memory_) return {};
    return thread_memory_->get_recent(n);
}

std::string MemoryManager::get_compressed_history() const {
    if (!compressed_history_) return "";
    return compressed_history_->get_combined();
}

void MemoryManager::store_fact(const std::string& ns, const std::string& key, const Json& value) {
    if (cross_thread_) {
        cross_thread_->store(ns, key, value);
        cross_thread_->save();
    }
}

std::optional<Json> MemoryManager::retrieve_fact(const std::string& ns, const std::string& key) const {
    if (!cross_thread_) return std::nullopt;
    return cross_thread_->retrieve(ns, key);
}

Result<void, Error> MemoryManager::store_episode(const Episode& episode) {
    if (!episodic_) {
        return Result<void, Error>::err(ErrorCode::InternalError, "Episodic memory not initialized");
    }
    return episodic_->store(episode);
}

std::vector<Episode> MemoryManager::retrieve_episodes(const std::string& query, int limit) const {
    if (!episodic_) return {};
    return episodic_->search(query, limit);
}

size_t MemoryManager::episode_count() const {
    if (!episodic_) return 0;
    return episodic_->count();
}

size_t MemoryManager::successful_episode_count() const {
    if (!episodic_) return 0;
    return episodic_->count_successful();
}

Result<CheckpointId, Error> MemoryManager::create_checkpoint(const std::string& description) {
    if (!session_state_ || !thread_memory_ || !compressed_history_ || !checkpointer_) {
        return Result<CheckpointId, Error>::err(
            ErrorCode::InternalError,
            "No active session or checkpointer not initialized"
        );
    }

    return checkpointer_->create(*session_state_, *thread_memory_, *compressed_history_, description, "manual");
}

Result<void, Error> MemoryManager::restore_checkpoint(const CheckpointId& id) {
    if (!checkpointer_) {
        return Result<void, Error>::err(ErrorCode::InternalError, "Checkpointer not initialized");
    }

    auto result = checkpointer_->restore(id);
    if (result.is_err()) {
        return Result<void, Error>::err(std::move(result).error());
    }

    auto checkpoint = std::move(result).value();
    session_state_ = std::move(checkpoint.session_state);
    thread_memory_ = std::move(checkpoint.thread_memory);
    compressed_history_ = std::move(checkpoint.compressed_history);
    current_session_id_ = checkpoint.info.session_id;

    return Result<void, Error>::ok();
}

std::vector<CheckpointInfo> MemoryManager::list_checkpoints() const {
    if (!checkpointer_ || !current_session_id_) return {};
    return checkpointer_->list(*current_session_id_);
}

std::string MemoryManager::get_user_memory() const {
    fs::path path = user_memory_path();
    if (!fs::exists(path)) return "";

    std::ifstream file(path);
    if (!file) return "";

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string MemoryManager::get_project_memory() const {
    fs::path path = project_memory_path();
    if (!fs::exists(path)) return "";

    std::ifstream file(path);
    if (!file) return "";

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void MemoryManager::update_user_memory(const std::string& content) {
    std::ofstream file(user_memory_path());
    if (file) {
        file << content;
    }
}

void MemoryManager::update_project_memory(const std::string& content) {
    std::ofstream file(project_memory_path());
    if (file) {
        file << content;
    }
}

Result<void, Error> MemoryManager::save_all() {
    if (!current_session_id_) {
        return Result<void, Error>::ok();
    }

    fs::path sess_path = session_path(*current_session_id_);

    // Save session state
    if (session_state_) {
        auto result = session_state_->save(sess_path / "state.json");
        if (result.is_err()) {
            return result;
        }
    }

    // Save thread memory
    if (thread_memory_) {
        auto result = thread_memory_->save(sess_path / "thread.jsonl");
        if (result.is_err()) {
            return result;
        }
    }

    // Save compressed history
    if (compressed_history_) {
        auto result = compressed_history_->save(sess_path / "history.json");
        if (result.is_err()) {
            return result;
        }
    }

    // Save cross-thread memory
    if (cross_thread_) {
        auto result = cross_thread_->save();
        if (result.is_err()) {
            return result;
        }
    }

    return Result<void, Error>::ok();
}

Result<void, Error> MemoryManager::load_all() {
    if (!current_session_id_) {
        return Result<void, Error>::err(ErrorCode::SessionNotFound, "No active session");
    }

    return resume_session(*current_session_id_);
}

Result<void, Error> MemoryManager::initialize() {
    // Start a default session if none exists
    if (!has_active_session()) {
        auto session_id = generate_session_id();
        return start_session(session_id);
    }
    return Result<void, Error>::ok();
}

void MemoryManager::add_message(const Message& message) {
    append_message(message);
}

}  // namespace gpagent::memory
