#include "gpagent/core/config.hpp"

#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <fstream>
#include <regex>

namespace gpagent::core {

std::string expand_path(const std::string& path) {
    std::string result = path;

    // Expand ~
    if (!result.empty() && result[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) {
            result = std::string(home) + result.substr(1);
        }
    }

    // Expand ${VAR} patterns
    std::regex env_regex(R"(\$\{([^}]+)\})");
    std::smatch match;
    while (std::regex_search(result, match, env_regex)) {
        std::string var_name = match[1].str();
        const char* var_value = std::getenv(var_name.c_str());
        std::string replacement = var_value ? var_value : "";
        result = match.prefix().str() + replacement + match.suffix().str();
    }

    // Expand $VAR patterns (without braces)
    std::regex env_regex2(R"(\$([A-Za-z_][A-Za-z0-9_]*))");
    while (std::regex_search(result, match, env_regex2)) {
        std::string var_name = match[1].str();
        const char* var_value = std::getenv(var_name.c_str());
        std::string replacement = var_value ? var_value : "";
        result = match.prefix().str() + replacement + match.suffix().str();
    }

    return result;
}

fs::path expand_path_fs(const fs::path& path) {
    return fs::path(expand_path(path.string()));
}

fs::path expand_path(const fs::path& path) {
    return expand_path_fs(path);
}

fs::path Config::default_path() {
    return fs::path(expand_path(std::string("~/.gpagent/config.yaml")));
}

void Config::expand_paths() {
    memory.storage_path = expand_path_fs(memory.storage_path);
    trm.model_path = expand_path_fs(trm.model_path);
    observability.log_path = expand_path_fs(observability.log_path);

    // Expand security allowed paths
    for (auto& path : security.allowed_paths) {
        path = expand_path(path);
    }
}

Result<void, Error> Config::validate() const {
    // Check for required API keys based on provider
    if (llm.primary_provider == "claude" && api_keys.anthropic.empty()) {
        return Result<void, Error>::err(
            ErrorCode::LLMApiKeyMissing,
            "Anthropic API key required for Claude provider"
        );
    }

    if (llm.primary_provider == "gemini" && api_keys.google.empty()) {
        return Result<void, Error>::err(
            ErrorCode::LLMApiKeyMissing,
            "Google API key required for Gemini provider"
        );
    }

    // Validate context settings
    if (context.max_tokens <= 0) {
        return Result<void, Error>::err(
            ErrorCode::ConfigValidationFailed,
            "context.max_tokens must be positive"
        );
    }

    if (context.compaction_threshold >= context.max_tokens) {
        return Result<void, Error>::err(
            ErrorCode::ConfigValidationFailed,
            "context.compaction_threshold must be less than max_tokens"
        );
    }

    // Validate TRM settings
    if (trm.enabled && trm.min_episodes_before_training < 1) {
        return Result<void, Error>::err(
            ErrorCode::ConfigValidationFailed,
            "trm.min_episodes_before_training must be at least 1"
        );
    }

    return Result<void, Error>::ok();
}

Result<Config, Error> Config::load(const fs::path& path) {
    fs::path expanded = expand_path_fs(path);

    if (!fs::exists(expanded)) {
        return Result<Config, Error>::err(
            ErrorCode::ConfigNotFound,
            "Configuration file not found",
            expanded.string()
        );
    }

    try {
        YAML::Node root = YAML::LoadFile(expanded.string());
        Config config;

        // Parse LLM config
        if (auto llm_node = root["llm"]) {
            config.llm.primary_provider = llm_node["primary_provider"].as<std::string>(config.llm.primary_provider);
            config.llm.primary_model = llm_node["primary_model"].as<std::string>(config.llm.primary_model);
            config.llm.fallback_provider = llm_node["fallback_provider"].as<std::string>(config.llm.fallback_provider);
            config.llm.fallback_model = llm_node["fallback_model"].as<std::string>(config.llm.fallback_model);
            config.llm.summarization_model = llm_node["summarization_model"].as<std::string>(config.llm.summarization_model);
            config.llm.max_retries = llm_node["max_retries"].as<int>(config.llm.max_retries);
            config.llm.timeout_ms = llm_node["timeout_ms"].as<int>(config.llm.timeout_ms);
            config.llm.temperature = llm_node["temperature"].as<double>(config.llm.temperature);
        }

        // Parse API keys (prefer environment variables)
        if (auto keys_node = root["api_keys"]) {
            config.api_keys.anthropic = expand_path(keys_node["anthropic"].as<std::string>(""));
            config.api_keys.google = expand_path(keys_node["google"].as<std::string>(""));
            config.api_keys.openai = expand_path(keys_node["openai"].as<std::string>(""));
            config.api_keys.tavily = expand_path(keys_node["tavily"].as<std::string>(""));
            config.api_keys.perplexity = expand_path(keys_node["perplexity"].as<std::string>(""));
            config.api_keys.google_search = expand_path(keys_node["google_search"].as<std::string>(""));
            config.api_keys.google_cx = keys_node["google_cx"].as<std::string>("");
        }

        // Override with environment variables if set
        if (const char* key = std::getenv("ANTHROPIC_API_KEY")) {
            config.api_keys.anthropic = key;
        }
        if (const char* key = std::getenv("GOOGLE_API_KEY")) {
            config.api_keys.google = key;
        }
        if (const char* key = std::getenv("OPENAI_API_KEY")) {
            config.api_keys.openai = key;
        }
        if (const char* key = std::getenv("TAVILY_API_KEY")) {
            config.api_keys.tavily = key;
        }
        if (const char* key = std::getenv("PERPLEXITY_API_KEY")) {
            config.api_keys.perplexity = key;
        }
        if (const char* key = std::getenv("GOOGLE_SEARCH_API_KEY")) {
            config.api_keys.google_search = key;
        }
        if (const char* key = std::getenv("GOOGLE_CX")) {
            config.api_keys.google_cx = key;
        }

        // Parse search config
        if (auto search_node = root["search"]) {
            config.search.provider = search_node["provider"].as<std::string>(config.search.provider);
            config.search.max_results = search_node["max_results"].as<int>(config.search.max_results);
            config.search.timeout_ms = search_node["timeout_ms"].as<int>(config.search.timeout_ms);
            config.search.safe_search = search_node["safe_search"].as<bool>(config.search.safe_search);
        }

        // Parse memory config
        if (auto mem_node = root["memory"]) {
            config.memory.storage_path = mem_node["storage_path"].as<std::string>(config.memory.storage_path.string());
            config.memory.max_episodes = mem_node["max_episodes"].as<int>(config.memory.max_episodes);
            config.memory.checkpoint_interval = mem_node["checkpoint_interval"].as<int>(config.memory.checkpoint_interval);
            config.memory.auto_checkpoint = mem_node["auto_checkpoint"].as<bool>(config.memory.auto_checkpoint);
        }

        // Parse context config
        if (auto ctx_node = root["context"]) {
            config.context.max_tokens = ctx_node["max_tokens"].as<int>(config.context.max_tokens);
            config.context.compaction_threshold = ctx_node["compaction_threshold"].as<int>(config.context.compaction_threshold);
            config.context.keep_raw_turns = ctx_node["keep_raw_turns"].as<int>(config.context.keep_raw_turns);
            config.context.summarize_batch = ctx_node["summarize_batch"].as<int>(config.context.summarize_batch);
        }

        // Parse TRM config
        if (auto trm_node = root["trm"]) {
            config.trm.enabled = trm_node["enabled"].as<bool>(config.trm.enabled);
            config.trm.mode = trm_node["mode"].as<std::string>(config.trm.mode);
            config.trm.model_path = trm_node["model_path"].as<std::string>(config.trm.model_path.string());
            config.trm.min_episodes_before_training = trm_node["min_episodes_before_training"].as<int>(config.trm.min_episodes_before_training);
            config.trm.hidden_size = trm_node["hidden_size"].as<int>(config.trm.hidden_size);
            config.trm.num_layers = trm_node["num_layers"].as<int>(config.trm.num_layers);
            config.trm.T = trm_node["T"].as<int>(config.trm.T);
            config.trm.n = trm_node["n"].as<int>(config.trm.n);
            config.trm.N_sup = trm_node["N_sup"].as<int>(config.trm.N_sup);
            config.trm.ema_decay = trm_node["ema_decay"].as<float>(config.trm.ema_decay);
            config.trm.retrain_interval_hours = trm_node["retrain_interval_hours"].as<int>(config.trm.retrain_interval_hours);
            config.trm.fallback_mode = trm_node["fallback_mode"].as<std::string>(config.trm.fallback_mode);

            if (auto weights_node = trm_node["loss_weights"]) {
                config.trm.loss_weights.contrastive = weights_node["contrastive"].as<float>(config.trm.loss_weights.contrastive);
                config.trm.loss_weights.next_action = weights_node["next_action"].as<float>(config.trm.loss_weights.next_action);
                config.trm.loss_weights.outcome = weights_node["outcome"].as<float>(config.trm.loss_weights.outcome);
                config.trm.loss_weights.masked = weights_node["masked"].as<float>(config.trm.loss_weights.masked);
            }
        }

        // Parse tools config
        if (auto tools_node = root["tools"]) {
            if (auto builtin_node = tools_node["builtin"]) {
                for (const auto& tool : builtin_node) {
                    std::string name = tool.first.as<std::string>();
                    ToolConfig tc;
                    if (tool.second.IsMap()) {
                        tc.enabled = tool.second["enabled"].as<bool>(true);
                        tc.max_lines = tool.second["max_lines"].as<int>(0);
                        tc.require_confirm = tool.second["require_confirm"].as<bool>(false);
                        tc.timeout_ms = tool.second["timeout_ms"].as<int>(60000);
                    }
                    config.tools.builtin[name] = tc;
                }
            }
        }

        // Parse training config
        if (auto train_node = root["training"]) {
            config.training.auto_collect = train_node["auto_collect"].as<bool>(config.training.auto_collect);
            config.training.min_episodes_for_training = train_node["min_episodes_for_training"].as<int>(config.training.min_episodes_for_training);
            config.training.train_interval_hours = train_node["train_interval_hours"].as<int>(config.training.train_interval_hours);
        }

        // Parse concurrency config
        if (auto conc_node = root["concurrency"]) {
            config.concurrency.thread_pool_size = conc_node["thread_pool_size"].as<int>(config.concurrency.thread_pool_size);
            config.concurrency.max_parallel_tools = conc_node["max_parallel_tools"].as<int>(config.concurrency.max_parallel_tools);
            config.concurrency.async_llm = conc_node["async_llm"].as<bool>(config.concurrency.async_llm);
        }

        // Parse security config
        if (auto sec_node = root["security"]) {
            config.security.bash_sandbox = sec_node["bash_sandbox"].as<bool>(config.security.bash_sandbox);
            config.security.max_file_size_mb = sec_node["max_file_size_mb"].as<int>(config.security.max_file_size_mb);

            if (auto paths_node = sec_node["allowed_paths"]) {
                config.security.allowed_paths.clear();
                for (const auto& p : paths_node) {
                    config.security.allowed_paths.push_back(p.as<std::string>());
                }
            }

            if (auto cmds_node = sec_node["blocked_commands"]) {
                config.security.blocked_commands.clear();
                for (const auto& c : cmds_node) {
                    config.security.blocked_commands.push_back(c.as<std::string>());
                }
            }
        }

        // Parse observability config
        if (auto obs_node = root["observability"]) {
            config.observability.log_level = obs_node["log_level"].as<std::string>(config.observability.log_level);
            config.observability.log_path = obs_node["log_path"].as<std::string>(config.observability.log_path.string());
            config.observability.metrics_enabled = obs_node["metrics_enabled"].as<bool>(config.observability.metrics_enabled);
            config.observability.metrics_port = obs_node["metrics_port"].as<int>(config.observability.metrics_port);
            config.observability.trace_enabled = obs_node["trace_enabled"].as<bool>(config.observability.trace_enabled);
        }

        // Expand all paths
        config.expand_paths();

        // Validate
        auto validation = config.validate();
        if (validation.is_err()) {
            return Result<Config, Error>::err(std::move(validation).error());
        }

        return Result<Config, Error>::ok(std::move(config));

    } catch (const YAML::Exception& e) {
        return Result<Config, Error>::err(
            ErrorCode::ConfigParseFailed,
            std::string("YAML parse error: ") + e.what(),
            expanded.string()
        );
    } catch (const std::exception& e) {
        return Result<Config, Error>::err(
            ErrorCode::ConfigParseFailed,
            e.what(),
            expanded.string()
        );
    }
}

Config Config::load_or_default(const fs::path& path) {
    auto result = load(path);
    if (result.is_ok()) {
        return std::move(result).value();
    }

    // Return default config with environment API keys
    Config config;

    if (const char* key = std::getenv("ANTHROPIC_API_KEY")) {
        config.api_keys.anthropic = key;
    }
    if (const char* key = std::getenv("GOOGLE_API_KEY")) {
        config.api_keys.google = key;
    }
    if (const char* key = std::getenv("OPENAI_API_KEY")) {
        config.api_keys.openai = key;
    }
    if (const char* key = std::getenv("TAVILY_API_KEY")) {
        config.api_keys.tavily = key;
    }
    if (const char* key = std::getenv("PERPLEXITY_API_KEY")) {
        config.api_keys.perplexity = key;
    }
    if (const char* key = std::getenv("GOOGLE_SEARCH_API_KEY")) {
        config.api_keys.google_search = key;
    }
    if (const char* key = std::getenv("GOOGLE_CX")) {
        config.api_keys.google_cx = key;
    }

    config.expand_paths();
    return config;
}

Result<void, Error> Config::save(const fs::path& path) const {
    try {
        fs::path expanded = expand_path_fs(path);

        // Create parent directories if needed
        if (expanded.has_parent_path()) {
            fs::create_directories(expanded.parent_path());
        }

        YAML::Emitter out;
        out << YAML::BeginMap;

        // LLM config
        out << YAML::Key << "llm" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "primary_provider" << YAML::Value << llm.primary_provider;
        out << YAML::Key << "primary_model" << YAML::Value << llm.primary_model;
        out << YAML::Key << "fallback_provider" << YAML::Value << llm.fallback_provider;
        out << YAML::Key << "fallback_model" << YAML::Value << llm.fallback_model;
        out << YAML::Key << "summarization_model" << YAML::Value << llm.summarization_model;
        out << YAML::Key << "temperature" << YAML::Value << llm.temperature;
        out << YAML::EndMap;

        // API keys
        out << YAML::Key << "api_keys" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "anthropic" << YAML::Value << api_keys.anthropic;
        out << YAML::Key << "google" << YAML::Value << api_keys.google;
        out << YAML::Key << "openai" << YAML::Value << api_keys.openai;
        out << YAML::Key << "tavily" << YAML::Value << api_keys.tavily;
        out << YAML::Key << "perplexity" << YAML::Value << api_keys.perplexity;
        out << YAML::Key << "google_search" << YAML::Value << api_keys.google_search;
        out << YAML::Key << "google_cx" << YAML::Value << api_keys.google_cx;
        out << YAML::EndMap;

        // Search config
        out << YAML::Key << "search" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "provider" << YAML::Value << search.provider;
        out << YAML::Key << "max_results" << YAML::Value << search.max_results;
        out << YAML::Key << "timeout_ms" << YAML::Value << search.timeout_ms;
        out << YAML::Key << "safe_search" << YAML::Value << search.safe_search;
        out << YAML::EndMap;

        // Memory config
        out << YAML::Key << "memory" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "storage_path" << YAML::Value << memory.storage_path.string();
        out << YAML::Key << "max_episodes" << YAML::Value << memory.max_episodes;
        out << YAML::Key << "checkpoint_interval" << YAML::Value << memory.checkpoint_interval;
        out << YAML::EndMap;

        // Context config
        out << YAML::Key << "context" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "max_tokens" << YAML::Value << context.max_tokens;
        out << YAML::Key << "compaction_threshold" << YAML::Value << context.compaction_threshold;
        out << YAML::Key << "keep_raw_turns" << YAML::Value << context.keep_raw_turns;
        out << YAML::Key << "summarize_batch" << YAML::Value << context.summarize_batch;
        out << YAML::EndMap;

        // TRM config
        out << YAML::Key << "trm" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "enabled" << YAML::Value << trm.enabled;
        out << YAML::Key << "mode" << YAML::Value << trm.mode;
        out << YAML::Key << "min_episodes_before_training" << YAML::Value << trm.min_episodes_before_training;
        out << YAML::Key << "fallback_mode" << YAML::Value << trm.fallback_mode;
        out << YAML::EndMap;

        // Observability config
        out << YAML::Key << "observability" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "log_level" << YAML::Value << observability.log_level;
        out << YAML::Key << "log_path" << YAML::Value << observability.log_path.string();
        out << YAML::Key << "metrics_enabled" << YAML::Value << observability.metrics_enabled;
        out << YAML::EndMap;

        out << YAML::EndMap;

        std::ofstream file(expanded);
        if (!file) {
            return Result<void, Error>::err(
                ErrorCode::FileWriteFailed,
                "Failed to open config file for writing",
                expanded.string()
            );
        }

        file << out.c_str();
        return Result<void, Error>::ok();

    } catch (const std::exception& e) {
        return Result<void, Error>::err(
            ErrorCode::FileWriteFailed,
            e.what(),
            path.string()
        );
    }
}

}  // namespace gpagent::core
