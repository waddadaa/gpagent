#include <catch2/catch_test_macros.hpp>
#include "gpagent/core/config.hpp"

using namespace gpagent::core;

TEST_CASE("Default config values", "[config]") {
    Config config;

    REQUIRE(config.llm.primary_provider == "claude");
    REQUIRE(config.llm.fallback_provider == "gemini");
}

TEST_CASE("Config LLM settings", "[config]") {
    LLMConfig llm_config;

    REQUIRE(llm_config.temperature >= 0.0);
    REQUIRE(llm_config.temperature <= 2.0);
    REQUIRE(llm_config.max_retries > 0);
}

TEST_CASE("Config memory settings", "[config]") {
    MemoryConfig mem_config;

    REQUIRE(mem_config.max_episodes > 0);
    REQUIRE(mem_config.checkpoint_interval > 0);
}
