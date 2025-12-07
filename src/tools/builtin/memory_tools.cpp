#include "gpagent/tools/tool_registry.hpp"

#include <fstream>
#include <filesystem>

namespace gpagent::tools::builtin {

namespace fs = std::filesystem;

// Note: These tools use a simple file-based storage.
// In production, they would integrate with the MemoryManager.

static fs::path get_memory_path(const ToolContext& ctx) {
    fs::path base = ctx.working_directory;
    if (const char* home = std::getenv("HOME")) {
        base = fs::path(home) / ".gpagent" / "memory";
    }
    fs::create_directories(base);
    return base;
}

ToolResult memory_store_handler(const Json& args, const ToolContext& ctx) {
    std::string key = args.at("key").get<std::string>();
    std::string value = args.at("value").get<std::string>();
    std::string ns = args.value("namespace", "default");

    // Validate key (alphanumeric and underscores only)
    for (char c : key) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Invalid key: only alphanumeric, underscore, and hyphen allowed"
            };
        }
    }

    try {
        fs::path memory_dir = get_memory_path(ctx) / ns;
        fs::create_directories(memory_dir);

        fs::path file_path = memory_dir / (key + ".txt");
        std::ofstream file(file_path);
        if (!file) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to write memory file"
            };
        }

        file << value;

        return ToolResult{
            .success = true,
            .content = "Stored '" + key + "' in namespace '" + ns + "'"
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error storing memory: ") + e.what()
        };
    }
}

ToolResult memory_retrieve_handler(const Json& args, const ToolContext& ctx) {
    std::string key = args.at("key").get<std::string>();
    std::string ns = args.value("namespace", "default");

    try {
        fs::path file_path = get_memory_path(ctx) / ns / (key + ".txt");

        if (!fs::exists(file_path)) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Key not found: " + key + " in namespace " + ns
            };
        }

        std::ifstream file(file_path);
        if (!file) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to read memory file"
            };
        }

        std::ostringstream ss;
        ss << file.rdbuf();

        return ToolResult{
            .success = true,
            .content = ss.str()
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error retrieving memory: ") + e.what()
        };
    }
}

ToolResult memory_list_handler(const Json& args, const ToolContext& ctx) {
    std::string ns = args.value("namespace", "default");

    try {
        fs::path memory_dir = get_memory_path(ctx) / ns;

        if (!fs::exists(memory_dir)) {
            return ToolResult{
                .success = true,
                .content = "No memories stored in namespace '" + ns + "'"
            };
        }

        std::ostringstream result;
        result << "Memories in namespace '" << ns << "':\n";

        for (const auto& entry : fs::directory_iterator(memory_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                std::string key = entry.path().stem().string();
                auto size = fs::file_size(entry.path());
                result << "  - " << key << " (" << size << " bytes)\n";
            }
        }

        return ToolResult{
            .success = true,
            .content = result.str()
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error listing memories: ") + e.what()
        };
    }
}

ToolResult memory_delete_handler(const Json& args, const ToolContext& ctx) {
    std::string key = args.at("key").get<std::string>();
    std::string ns = args.value("namespace", "default");

    try {
        fs::path file_path = get_memory_path(ctx) / ns / (key + ".txt");

        if (!fs::exists(file_path)) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Key not found: " + key
            };
        }

        fs::remove(file_path);

        return ToolResult{
            .success = true,
            .content = "Deleted '" + key + "' from namespace '" + ns + "'"
        };

    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error deleting memory: ") + e.what()
        };
    }
}

// Register memory tools
void register_memory_tools(ToolRegistry& registry) {
    registry.register_tool(
        ToolSpec{
            .name = "memory_store",
            .description = "Store a value in persistent memory for later retrieval.",
            .parameters = {
                {"key", "The key to store the value under", ParamType::String, true},
                {"value", "The value to store", ParamType::String, true},
                {"namespace", "Namespace for organization (default: 'default')", ParamType::String, false}
            },
            .keywords = {"memory", "store", "save", "remember", "persist"}
        },
        memory_store_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "memory_retrieve",
            .description = "Retrieve a previously stored value from memory.",
            .parameters = {
                {"key", "The key to retrieve", ParamType::String, true},
                {"namespace", "Namespace to search in (default: 'default')", ParamType::String, false}
            },
            .keywords = {"memory", "retrieve", "get", "recall", "fetch"}
        },
        memory_retrieve_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "memory_list",
            .description = "List all stored memories in a namespace.",
            .parameters = {
                {"namespace", "Namespace to list (default: 'default')", ParamType::String, false}
            },
            .keywords = {"memory", "list", "show", "keys"}
        },
        memory_list_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "memory_delete",
            .description = "Delete a stored memory.",
            .parameters = {
                {"key", "The key to delete", ParamType::String, true},
                {"namespace", "Namespace (default: 'default')", ParamType::String, false}
            },
            .keywords = {"memory", "delete", "remove", "forget"}
        },
        memory_delete_handler,
        "builtin"
    );
}

}  // namespace gpagent::tools::builtin
