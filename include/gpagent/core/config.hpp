#pragma once

#include "errors.hpp"
#include "result.hpp"
#include "types.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace gpagent::core {

namespace fs = std::filesystem;

// LLM provider configuration
struct LLMConfig {
    std::string primary_provider = "claude";
    std::string primary_model = "claude-opus-4-5-20251101";
    std::string fallback_provider = "gemini";
    std::string fallback_model = "gemini-3-pro-preview";
    std::string summarization_model = "claude-3-5-haiku-20241022";

    int max_retries = 3;
    int timeout_ms = 120000;
    double temperature = 0.7;
};

// API keys configuration
struct ApiKeysConfig {
    std::string anthropic;  // From env: ANTHROPIC_API_KEY
    std::string google;     // From env: GOOGLE_API_KEY
    std::string openai;     // From env: OPENAI_API_KEY
    std::string tavily;     // From env: TAVILY_API_KEY
    std::string perplexity; // From env: PERPLEXITY_API_KEY
    std::string google_search;  // From env: GOOGLE_SEARCH_API_KEY (Custom Search)
    std::string google_cx;      // Google Custom Search Engine ID
};

// Web search provider configuration
struct SearchConfig {
    // Provider: perplexity (default), google, tavily
    std::string provider = "perplexity";
    int max_results = 10;
    int timeout_ms = 30000;
    bool safe_search = true;
};

// Memory configuration
struct MemoryConfig {
    fs::path storage_path = "~/.gpagent/storage";
    fs::path data_dir = "~/.gpagent/data";  // Alternative path for data storage
    int max_episodes = 10000;
    int checkpoint_interval = 10;  // turns
    bool auto_checkpoint = true;
};

// Context configuration
struct ContextConfig {
    int max_tokens = 180000;
    int compaction_threshold = 150000;
    int keep_raw_turns = 10;
    int summarize_batch = 21;
    int reserved_for_response = 30000;
};

// TRM loss weights for unsupervised learning
struct TRMLossWeights {
    float contrastive = 1.0f;
    float next_action = 0.5f;
    float outcome = 0.3f;
    float masked = 0.2f;
};

// TRM configuration
struct TRMConfig {
    bool enabled = true;
    std::string mode = "unsupervised";  // unsupervised | supervised
    fs::path model_path = "~/.gpagent/models/trm_tool_selector.pt";
    int min_episodes_before_training = 5;  // Low threshold for unsupervised learning

    // Model architecture
    int hidden_size = 512;
    int num_layers = 2;
    int T = 3;
    int n = 6;
    int N_sup = 16;

    // Training
    int epochs = 10;
    float learning_rate = 0.001f;
    float ema_decay = 0.999f;
    int retrain_interval_hours = 24;
    std::string fallback_mode = "rules";  // rules | keyword | disabled

    TRMLossWeights loss_weights;
};

// Tool configuration
struct ToolConfig {
    bool enabled = true;
    int max_lines = 2000;
    bool require_confirm = false;
    int timeout_ms = 120000;
};

struct ToolsConfig {
    std::map<std::string, ToolConfig> builtin;
    std::vector<Json> mcp_servers;

    ToolsConfig() {
        // Default builtin tools
        builtin["file_read"] = {true, 2000, false, 60000};
        builtin["file_write"] = {true, 0, true, 60000};
        builtin["file_edit"] = {true, 0, true, 60000};
        builtin["bash"] = {true, 0, false, 120000};
        builtin["grep"] = {true, 0, false, 60000};
        builtin["glob"] = {true, 0, false, 60000};
        builtin["web_search"] = {true, 0, false, 30000};
        builtin["web_fetch"] = {true, 0, false, 30000};
    }
};

// Training configuration
struct TrainingConfig {
    bool auto_collect = true;
    int min_episodes_for_training = 100;
    int train_interval_hours = 24;
    float learning_rate = 1e-4f;
    int batch_size = 64;
};

// Concurrency configuration
struct ConcurrencyConfig {
    int thread_pool_size = 4;
    int max_parallel_tools = 4;
    bool async_llm = true;
};

// Security configuration
struct SecurityConfig {
    bool bash_sandbox = true;
    std::vector<std::string> allowed_paths;
    std::vector<std::string> blocked_commands;
    int max_file_size_mb = 100;

    SecurityConfig() {
        // Default allowed paths (will be expanded at runtime)
        allowed_paths = {"${HOME}", "${CWD}", "/tmp"};
        blocked_commands = {"rm -rf /", "sudo", "> /dev/sd", "dd if=/dev/zero"};
    }
};

// Observability configuration
struct ObservabilityConfig {
    std::string log_level = "info";  // debug, info, warn, error
    fs::path log_path = "~/.gpagent/logs";
    bool metrics_enabled = true;
    int metrics_port = 9090;
    bool trace_enabled = false;
};

// Main configuration
struct Config {
    LLMConfig llm;
    ApiKeysConfig api_keys;
    SearchConfig search;
    MemoryConfig memory;
    ContextConfig context;
    TRMConfig trm;
    ToolsConfig tools;
    TrainingConfig training;
    ConcurrencyConfig concurrency;
    SecurityConfig security;
    ObservabilityConfig observability;

    // Load configuration from file
    static Result<Config, Error> load(const fs::path& path);

    // Load with defaults, falling back if file doesn't exist
    static Config load_or_default(const fs::path& path);

    // Save configuration to file
    Result<void, Error> save(const fs::path& path) const;

    // Get default config path
    static fs::path default_path();

    // Expand environment variables in paths
    void expand_paths();

    // Validate configuration
    Result<void, Error> validate() const;
};

// Helper to expand ~ and environment variables in paths
std::string expand_path(const std::string& path);
fs::path expand_path(const fs::path& path);

}  // namespace gpagent::core
