#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <iomanip>

namespace gpagent::core {

// UUID v4 implementation
class UUID {
public:
    UUID() : bytes_{} {}

    // Generate a new random UUID (v4)
    static UUID generate() {
        UUID uuid;

        // Use random_device for seeding, then mt19937_64 for generation
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        static thread_local std::uniform_int_distribution<uint64_t> dist;

        // Generate 128 bits of randomness
        uint64_t high = dist(gen);
        uint64_t low = dist(gen);

        // Set version (4) and variant (RFC 4122)
        high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;  // Version 4
        low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;   // Variant 1

        // Copy to bytes
        for (int i = 0; i < 8; ++i) {
            uuid.bytes_[i] = static_cast<uint8_t>((high >> (56 - i * 8)) & 0xFF);
            uuid.bytes_[i + 8] = static_cast<uint8_t>((low >> (56 - i * 8)) & 0xFF);
        }

        return uuid;
    }

    // Parse from string (format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
    static UUID from_string(const std::string& str) {
        UUID uuid;

        if (str.length() != 36) {
            return uuid;  // Return null UUID
        }

        size_t byte_idx = 0;
        for (size_t i = 0; i < str.length() && byte_idx < 16; i += 2) {
            if (str[i] == '-') {
                --i;
                continue;
            }

            char hex[3] = {str[i], str[i + 1], '\0'};
            uuid.bytes_[byte_idx++] = static_cast<uint8_t>(std::strtoul(hex, nullptr, 16));
        }

        return uuid;
    }

    // Convert to string format
    std::string to_string() const {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');

        for (size_t i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                ss << '-';
            }
            ss << std::setw(2) << static_cast<int>(bytes_[i]);
        }

        return ss.str();
    }

    // Check if UUID is valid (non-zero)
    bool is_valid() const {
        for (auto b : bytes_) {
            if (b != 0) return true;
        }
        return false;
    }

    // Comparison operators
    bool operator==(const UUID& other) const {
        return bytes_ == other.bytes_;
    }

    bool operator!=(const UUID& other) const {
        return bytes_ != other.bytes_;
    }

    bool operator<(const UUID& other) const {
        return bytes_ < other.bytes_;
    }

    // Get raw bytes
    const std::array<uint8_t, 16>& bytes() const { return bytes_; }

private:
    std::array<uint8_t, 16> bytes_;
};

// Convenience functions for prefixed IDs
inline std::string generate_session_id() {
    return "sess_" + UUID::generate().to_string().substr(0, 8);
}

inline std::string generate_episode_id() {
    return "ep_" + UUID::generate().to_string().substr(0, 8);
}

inline std::string generate_checkpoint_id() {
    return "cp_" + UUID::generate().to_string().substr(0, 8);
}

inline std::string generate_thread_id() {
    return "thread_" + UUID::generate().to_string().substr(0, 8);
}

inline std::string generate_tool_call_id() {
    return "tc_" + UUID::generate().to_string().substr(0, 12);
}

}  // namespace gpagent::core
