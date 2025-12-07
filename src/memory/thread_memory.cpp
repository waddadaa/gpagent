#include "gpagent/memory/thread_memory.hpp"
#include "gpagent/core/uuid.hpp"

#include <fstream>
#include <sstream>

namespace gpagent::memory {

// ThreadMemory
ThreadMemory::ThreadMemory()
    : thread_id_(generate_thread_id())
{
}

ThreadMemory::ThreadMemory(const ThreadId& id)
    : thread_id_(id)
{
}

void ThreadMemory::append(const Message& message) {
    messages_.push_back(message);
}

void ThreadMemory::append(Message&& message) {
    messages_.push_back(std::move(message));
}

std::vector<Message> ThreadMemory::get_recent(size_t n) const {
    if (n >= messages_.size()) {
        return {messages_.begin(), messages_.end()};
    }
    return {messages_.end() - n, messages_.end()};
}

std::vector<Message> ThreadMemory::get_range(size_t start, size_t end) const {
    if (start >= messages_.size()) return {};
    end = std::min(end, messages_.size());
    return {messages_.begin() + start, messages_.begin() + end};
}

void ThreadMemory::trim(size_t keep_last) {
    if (messages_.size() > keep_last) {
        size_t to_remove = messages_.size() - keep_last;
        for (size_t i = 0; i < to_remove; ++i) {
            messages_.pop_front();
        }
    }
}

Result<void, Error> ThreadMemory::save(const fs::path& path) const {
    try {
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

        // Write as JSONL (one JSON object per line)
        for (const auto& msg : messages_) {
            file << msg.to_json().dump() << "\n";
        }

        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what(),
            path.string()
        );
    }
}

Result<ThreadMemory, Error> ThreadMemory::load(const fs::path& path) {
    try {
        if (!fs::exists(path)) {
            return Result<ThreadMemory, Error>::err(
                ErrorCode::FileNotFound,
                "Thread memory file not found",
                path.string()
            );
        }

        std::ifstream file(path);
        if (!file) {
            return Result<ThreadMemory, Error>::err(
                ErrorCode::FileReadFailed,
                "Failed to open file for reading",
                path.string()
            );
        }

        ThreadMemory memory;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            try {
                Json j = Json::parse(line);
                memory.append(Message::from_json(j));
            } catch (const Json::exception& e) {
                // Log warning but continue with other lines
                continue;
            }
        }

        return Result<ThreadMemory, Error>::ok(std::move(memory));

    } catch (const std::exception& e) {
        return Result<ThreadMemory, Error>::err(
            ErrorCode::FileReadFailed,
            e.what(),
            path.string()
        );
    }
}

Result<void, Error> ThreadMemory::append_to_file(const fs::path& path, const Message& message) const {
    try {
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
        }

        std::ofstream file(path, std::ios::app);
        if (!file) {
            return Result<void, Error>::err(
                ErrorCode::FileWriteFailed,
                "Failed to open file for appending",
                path.string()
            );
        }

        file << message.to_json().dump() << "\n";
        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what(),
            path.string()
        );
    }
}

// CompressedHistory
Json CompressedHistory::Summary::to_json() const {
    return Json{
        {"start_turn", start_turn},
        {"end_turn", end_turn},
        {"content", content},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            created_at.time_since_epoch()).count()}
    };
}

CompressedHistory::Summary CompressedHistory::Summary::from_json(const Json& j) {
    Summary s;
    s.start_turn = j.value("start_turn", 0);
    s.end_turn = j.value("end_turn", 0);
    s.content = j.value("content", "");
    if (j.contains("created_at")) {
        s.created_at = TimePoint{std::chrono::seconds{j["created_at"].get<int64_t>()}};
    }
    return s;
}

void CompressedHistory::add_summary(size_t start_turn, size_t end_turn, std::string content) {
    summaries_.push_back(Summary{
        .start_turn = start_turn,
        .end_turn = end_turn,
        .content = std::move(content),
        .created_at = Clock::now()
    });
}

std::string CompressedHistory::get_combined() const {
    std::ostringstream ss;
    for (size_t i = 0; i < summaries_.size(); ++i) {
        if (i > 0) ss << "\n\n";
        ss << "[Turns " << summaries_[i].start_turn << "-" << summaries_[i].end_turn << "]\n";
        ss << summaries_[i].content;
    }
    return ss.str();
}

Result<void, Error> CompressedHistory::save(const fs::path& path) const {
    try {
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

        Json j = Json::array();
        for (const auto& s : summaries_) {
            j.push_back(s.to_json());
        }

        file << j.dump(2);
        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what(),
            path.string()
        );
    }
}

Result<CompressedHistory, Error> CompressedHistory::load(const fs::path& path) {
    try {
        if (!fs::exists(path)) {
            // Not an error - just return empty history
            return Result<CompressedHistory, Error>::ok(CompressedHistory{});
        }

        std::ifstream file(path);
        if (!file) {
            return Result<CompressedHistory, Error>::err(
                ErrorCode::FileReadFailed,
                "Failed to open file for reading",
                path.string()
            );
        }

        Json j = Json::parse(file);
        CompressedHistory history;

        for (const auto& item : j) {
            history.summaries_.push_back(Summary::from_json(item));
        }

        return Result<CompressedHistory, Error>::ok(std::move(history));

    } catch (const Json::exception& e) {
        return Result<CompressedHistory, Error>::err(
            ErrorCode::MemoryCorrupted,
            std::string("JSON parse error: ") + e.what(),
            path.string()
        );
    } catch (const std::exception& e) {
        return Result<CompressedHistory, Error>::err(
            ErrorCode::FileReadFailed,
            e.what(),
            path.string()
        );
    }
}

}  // namespace gpagent::memory
