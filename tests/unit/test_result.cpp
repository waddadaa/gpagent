#include <catch2/catch_test_macros.hpp>
#include "gpagent/core/result.hpp"

using namespace gpagent::core;

TEST_CASE("Result with value", "[result]") {
    auto result = Result<int, std::string>::ok(42);

    REQUIRE(result.is_ok());
    REQUIRE_FALSE(result.is_err());
    REQUIRE(result.value() == 42);
}

TEST_CASE("Result with error", "[result]") {
    auto result = Result<int, std::string>::err("something went wrong");

    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.is_err());
    REQUIRE(result.error() == "something went wrong");
}

TEST_CASE("Result void success", "[result]") {
    auto result = Result<void, std::string>::ok();

    REQUIRE(result.is_ok());
    REQUIRE_FALSE(result.is_err());
}

TEST_CASE("Result void error", "[result]") {
    auto result = Result<void, std::string>::err("error message");

    REQUIRE_FALSE(result.is_ok());
    REQUIRE(result.is_err());
    REQUIRE(result.error() == "error message");
}
