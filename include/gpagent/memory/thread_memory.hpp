#pragma once

#include "gpagent/core/types.hpp"
#include "gpagent/core/result.hpp"

#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace gpagent::memory {

using namespace gpagent::core;
namespace fs = std::filesystem;

// Thread memory - conversation history for a session
class ThreadMemory {
public:
    ThreadMemory();
    explicit ThreadMemory(const ThreadId& id);

    // Accessors
    const ThreadId& id() const { return thread_id_; }
    size_t size() const { return messages_.size(); }
    bool empty() const { return messages_.empty(); }

    // Message management
    void append(const Message& message);
    void append(Message&& message);

    // Get recent messages (last N turns)
    std::vector<Message> get_recent(size_t n) const;

    // Get all messages
    const std::deque<Message>& messages() const { return messages_; }

    // Get messages in range [start, end)
    std::vector<Message> get_range(size_t start, size_t end) const;

    // Clear old messages (keep last n)
    void trim(size_t keep_last);

    // Serialization - JSONL format (one message per line)
    Result<void, Error> save(const fs::path& path) const;
    static Result<ThreadMemory, Error> load(const fs::path& path);

    // Append single message to file (for incremental saves)
    Result<void, Error> append_to_file(const fs::path& path, const Message& message) const;

private:
    ThreadId thread_id_;
    std::deque<Message> messages_;
};

// Compressed history - summaries of older conversation turns
class CompressedHistory {
public:
    struct Summary {
        size_t start_turn;
        size_t end_turn;
        std::string content;
        TimePoint created_at;

        Json to_json() const;
        static Summary from_json(const Json& j);
    };

    CompressedHistory() = default;

    // Add a new summary
    void add_summary(size_t start_turn, size_t end_turn, std::string content);

    // Get all summaries
    const std::vector<Summary>& summaries() const { return summaries_; }

    // Get combined summary text
    std::string get_combined() const;

    // Serialization
    Result<void, Error> save(const fs::path& path) const;
    static Result<CompressedHistory, Error> load(const fs::path& path);

private:
    std::vector<Summary> summaries_;
};

}  // namespace gpagent::memory
