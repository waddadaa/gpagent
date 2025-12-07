#include <catch2/catch_test_macros.hpp>
#include "gpagent/core/uuid.hpp"

#include <set>

using namespace gpagent::core;

TEST_CASE("UUID generation", "[uuid]") {
    auto uuid1 = UUID::generate();
    auto uuid2 = UUID::generate();

    REQUIRE(uuid1.to_string() != uuid2.to_string());
    REQUIRE(uuid1.to_string().length() == 36);  // Standard UUID format
}

TEST_CASE("UUID uniqueness", "[uuid]") {
    std::set<std::string> uuids;

    for (int i = 0; i < 1000; ++i) {
        uuids.insert(UUID::generate().to_string());
    }

    REQUIRE(uuids.size() == 1000);
}

TEST_CASE("UUID from string", "[uuid]") {
    std::string uuid_str = "550e8400-e29b-41d4-a716-446655440000";
    auto uuid = UUID::from_string(uuid_str);

    REQUIRE(uuid.is_valid());
    REQUIRE(uuid.to_string() == uuid_str);
}

TEST_CASE("UUID invalid string", "[uuid]") {
    auto uuid = UUID::from_string("not-a-valid-uuid");

    REQUIRE_FALSE(uuid.is_valid());
}
