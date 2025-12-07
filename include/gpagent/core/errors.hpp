#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace gpagent::core {

// Error codes organized by category
enum class ErrorCode {
    // Success
    Ok = 0,

    // General errors (1-99)
    Unknown = 1,
    InvalidArgument = 2,
    NotFound = 3,
    AlreadyExists = 4,
    PermissionDenied = 5,
    Timeout = 6,
    Cancelled = 7,
    NotImplemented = 8,
    InternalError = 9,
    InvalidState = 10,

    // Memory errors (100-199)
    MemoryLoadFailed = 100,
    MemorySaveFailed = 101,
    MemoryCorrupted = 102,
    CheckpointNotFound = 103,
    EpisodeNotFound = 104,
    SessionExpired = 105,
    SessionNotFound = 106,

    // LLM errors (200-299)
    LLMConnectionFailed = 200,
    LLMRateLimited = 201,
    LLMContextOverflow = 202,
    LLMInvalidResponse = 203,
    LLMApiKeyMissing = 204,
    LLMProviderUnavailable = 205,
    LLMTokenLimitExceeded = 206,
    LLMStreamError = 207,

    // Tool errors (300-399)
    ToolNotFound = 300,
    ToolExecutionFailed = 301,
    ToolValidationFailed = 302,
    ToolTimeout = 303,
    ToolPermissionDenied = 304,
    MCPConnectionFailed = 305,
    MCPProtocolError = 306,
    ToolDisabled = 307,

    // TRM errors (400-499)
    TRMModelNotLoaded = 400,
    TRMInferenceFailed = 401,
    TRMTrainingFailed = 402,
    TRMInsufficientData = 403,
    TRMModelCorrupted = 404,

    // Context errors (500-599)
    ContextBuildFailed = 500,
    ContextCompactionFailed = 501,
    ContextTooLarge = 502,

    // Configuration errors (600-699)
    ConfigNotFound = 600,
    ConfigParseFailed = 601,
    ConfigValidationFailed = 602,
    ConfigKeyMissing = 603,

    // File system errors (700-799)
    FileNotFound = 700,
    FileReadFailed = 701,
    FileWriteFailed = 702,
    DirectoryNotFound = 703,
    PathNotAllowed = 704,
    FileTooLarge = 705,

    // Network errors (800-899)
    NetworkError = 800,
    ConnectionRefused = 801,
    DNSResolutionFailed = 802,
    SSLError = 803,
};

// Get human-readable message for error code
inline std::string_view error_code_message(ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok: return "Success";
        case ErrorCode::Unknown: return "Unknown error";
        case ErrorCode::InvalidArgument: return "Invalid argument";
        case ErrorCode::NotFound: return "Not found";
        case ErrorCode::AlreadyExists: return "Already exists";
        case ErrorCode::PermissionDenied: return "Permission denied";
        case ErrorCode::Timeout: return "Operation timed out";
        case ErrorCode::Cancelled: return "Operation cancelled";
        case ErrorCode::NotImplemented: return "Not implemented";
        case ErrorCode::InternalError: return "Internal error";
        case ErrorCode::InvalidState: return "Invalid state";

        case ErrorCode::MemoryLoadFailed: return "Failed to load memory";
        case ErrorCode::MemorySaveFailed: return "Failed to save memory";
        case ErrorCode::MemoryCorrupted: return "Memory data corrupted";
        case ErrorCode::CheckpointNotFound: return "Checkpoint not found";
        case ErrorCode::EpisodeNotFound: return "Episode not found";
        case ErrorCode::SessionExpired: return "Session expired";
        case ErrorCode::SessionNotFound: return "Session not found";

        case ErrorCode::LLMConnectionFailed: return "Failed to connect to LLM provider";
        case ErrorCode::LLMRateLimited: return "LLM rate limit exceeded";
        case ErrorCode::LLMContextOverflow: return "Context window exceeded";
        case ErrorCode::LLMInvalidResponse: return "Invalid response from LLM";
        case ErrorCode::LLMApiKeyMissing: return "API key not configured";
        case ErrorCode::LLMProviderUnavailable: return "LLM provider unavailable";
        case ErrorCode::LLMTokenLimitExceeded: return "Token limit exceeded";
        case ErrorCode::LLMStreamError: return "Streaming error";

        case ErrorCode::ToolNotFound: return "Tool not found";
        case ErrorCode::ToolExecutionFailed: return "Tool execution failed";
        case ErrorCode::ToolValidationFailed: return "Tool parameter validation failed";
        case ErrorCode::ToolTimeout: return "Tool execution timed out";
        case ErrorCode::ToolPermissionDenied: return "Tool permission denied";
        case ErrorCode::MCPConnectionFailed: return "MCP server connection failed";
        case ErrorCode::MCPProtocolError: return "MCP protocol error";
        case ErrorCode::ToolDisabled: return "Tool is disabled";

        case ErrorCode::TRMModelNotLoaded: return "TRM model not loaded";
        case ErrorCode::TRMInferenceFailed: return "TRM inference failed";
        case ErrorCode::TRMTrainingFailed: return "TRM training failed";
        case ErrorCode::TRMInsufficientData: return "Insufficient training data";
        case ErrorCode::TRMModelCorrupted: return "TRM model file corrupted";

        case ErrorCode::ContextBuildFailed: return "Failed to build context";
        case ErrorCode::ContextCompactionFailed: return "Context compaction failed";
        case ErrorCode::ContextTooLarge: return "Context too large";

        case ErrorCode::ConfigNotFound: return "Configuration file not found";
        case ErrorCode::ConfigParseFailed: return "Failed to parse configuration";
        case ErrorCode::ConfigValidationFailed: return "Configuration validation failed";
        case ErrorCode::ConfigKeyMissing: return "Required configuration key missing";

        case ErrorCode::FileNotFound: return "File not found";
        case ErrorCode::FileReadFailed: return "Failed to read file";
        case ErrorCode::FileWriteFailed: return "Failed to write file";
        case ErrorCode::DirectoryNotFound: return "Directory not found";
        case ErrorCode::PathNotAllowed: return "Path not allowed";
        case ErrorCode::FileTooLarge: return "File too large";

        case ErrorCode::NetworkError: return "Network error";
        case ErrorCode::ConnectionRefused: return "Connection refused";
        case ErrorCode::DNSResolutionFailed: return "DNS resolution failed";
        case ErrorCode::SSLError: return "SSL/TLS error";
    }
    return "Unknown error code";
}

// Check if error is retriable
inline bool is_retriable(ErrorCode code) {
    switch (code) {
        case ErrorCode::LLMRateLimited:
        case ErrorCode::LLMConnectionFailed:
        case ErrorCode::LLMStreamError:
        case ErrorCode::ToolTimeout:
        case ErrorCode::MCPConnectionFailed:
        case ErrorCode::NetworkError:
        case ErrorCode::ConnectionRefused:
        case ErrorCode::Timeout:
            return true;
        default:
            return false;
    }
}

// Check if error is fatal (no recovery possible)
inline bool is_fatal(ErrorCode code) {
    switch (code) {
        case ErrorCode::LLMApiKeyMissing:
        case ErrorCode::ConfigParseFailed:
        case ErrorCode::ConfigValidationFailed:
        case ErrorCode::MemoryCorrupted:
        case ErrorCode::PathNotAllowed:
            return true;
        default:
            return false;
    }
}

// Error structure with context
struct Error {
    ErrorCode code;
    std::string message;
    std::optional<std::string> context;  // Additional context (file path, tool name, etc.)
    std::optional<std::string> source;   // Source location or component

    Error() : code(ErrorCode::Unknown) {}

    Error(ErrorCode c) : code(c), message(std::string(error_code_message(c))) {}

    Error(ErrorCode c, std::string msg)
        : code(c), message(std::move(msg)) {}

    Error(ErrorCode c, std::string msg, std::string ctx)
        : code(c), message(std::move(msg)), context(std::move(ctx)) {}

    // Factory methods
    static Error from_code(ErrorCode code) {
        return Error{code};
    }

    static Error from_code(ErrorCode code, std::string context) {
        Error e{code};
        e.context = std::move(context);
        return e;
    }

    static Error from_exception(const std::exception& e) {
        return Error{ErrorCode::InternalError, e.what()};
    }

    // Predicates
    bool is_retriable() const { return gpagent::core::is_retriable(code); }
    bool is_fatal() const { return gpagent::core::is_fatal(code); }
    bool is_ok() const { return code == ErrorCode::Ok; }

    // Get full error message
    std::string full_message() const {
        std::string result = message;
        if (context) {
            result += " [" + *context + "]";
        }
        if (source) {
            result += " at " + *source;
        }
        return result;
    }

    // For logging
    std::string to_string() const {
        return "[" + std::to_string(static_cast<int>(code)) + "] " + full_message();
    }
};

}  // namespace gpagent::core
