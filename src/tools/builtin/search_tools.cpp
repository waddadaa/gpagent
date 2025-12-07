#include "gpagent/tools/tool_registry.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace gpagent::tools::builtin {

namespace fs = std::filesystem;

ToolResult grep_handler(const Json& args, const ToolContext& ctx) {
    std::string pattern_str = args.at("pattern").get<std::string>();
    std::string path = args.value("path", ctx.working_directory);
    std::string glob_filter = args.value("glob", "");
    std::string output_mode = args.value("output_mode", "files_with_matches");

    fs::path search_path(path);

    // Build regex
    std::regex pattern;
    try {
        pattern = std::regex(pattern_str, std::regex::ECMAScript);
    } catch (const std::regex_error& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Invalid regex pattern: ") + e.what()
        };
    }

    // Build glob filter regex if provided
    std::regex glob_regex;
    bool has_glob = !glob_filter.empty();
    if (has_glob) {
        try {
            std::string glob_regex_str = glob_filter;
            glob_regex_str = std::regex_replace(glob_regex_str, std::regex(R"([\.\+\^\$\|\(\)\[\]\{\}\\])"), R"(\$&)");
            glob_regex_str = std::regex_replace(glob_regex_str, std::regex(R"(\*\*)"), ".*");
            glob_regex_str = std::regex_replace(glob_regex_str, std::regex(R"(\*)"), "[^/]*");
            glob_regex = std::regex(glob_regex_str);
        } catch (...) {
            has_glob = false;
        }
    }

    std::vector<std::pair<std::string, std::vector<std::pair<int, std::string>>>> matches;
    int total_matches = 0;
    const int max_matches = 100;
    const int max_files = 50;

    auto process_file = [&](const fs::path& file_path) {
        if (total_matches >= max_matches || matches.size() >= max_files) {
            return;
        }

        // Apply glob filter
        if (has_glob) {
            std::string filename = file_path.filename().string();
            if (!std::regex_match(filename, glob_regex)) {
                return;
            }
        }

        std::ifstream file(file_path);
        if (!file) return;

        std::vector<std::pair<int, std::string>> file_matches;
        std::string line;
        int line_num = 0;

        while (std::getline(file, line) && total_matches < max_matches) {
            line_num++;
            if (std::regex_search(line, pattern)) {
                file_matches.emplace_back(line_num, line);
                total_matches++;
            }
        }

        if (!file_matches.empty()) {
            matches.emplace_back(file_path.string(), std::move(file_matches));
        }
    };

    try {
        if (fs::is_regular_file(search_path)) {
            process_file(search_path);
        } else if (fs::is_directory(search_path)) {
            for (const auto& entry : fs::recursive_directory_iterator(search_path,
                    fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;

                // Skip binary files and large files
                auto size = entry.file_size();
                if (size > 10 * 1024 * 1024) continue;  // Skip files > 10MB

                process_file(entry.path());

                if (total_matches >= max_matches || matches.size() >= max_files) {
                    break;
                }
            }
        }
    } catch (const std::exception& e) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error searching: ") + e.what()
        };
    }

    // Format output based on mode
    std::ostringstream result;

    if (output_mode == "files_with_matches") {
        for (const auto& [file, _] : matches) {
            result << file << "\n";
        }
    } else if (output_mode == "count") {
        for (const auto& [file, file_matches] : matches) {
            result << file << ":" << file_matches.size() << "\n";
        }
    } else {  // content
        for (const auto& [file, file_matches] : matches) {
            for (const auto& [line_num, line] : file_matches) {
                std::string truncated = line.length() > 200 ?
                    line.substr(0, 200) + "..." : line;
                result << file << ":" << line_num << ":" << truncated << "\n";
            }
        }
    }

    std::string output = result.str();
    if (output.empty()) {
        output = "No matches found";
    } else if (total_matches >= max_matches) {
        output += "\n... [results limited to " + std::to_string(max_matches) + " matches]";
    }

    return ToolResult{
        .success = true,
        .content = output
    };
}

void register_search_tools(ToolRegistry& registry) {
    registry.register_tool(
        ToolSpec{
            .name = "grep",
            .description = "Search for a regex pattern in files. Returns matching lines with file paths and line numbers.",
            .parameters = {
                {"pattern", "The regex pattern to search for", ParamType::String, true},
                {"path", "File or directory to search in (default: working directory)", ParamType::String, false},
                {"glob", "Glob pattern to filter files (e.g., *.cpp, *.py)", ParamType::String, false},
                {"output_mode", "Output mode: content (default), files_with_matches, or count", ParamType::String, false,
                    std::nullopt, std::vector<std::string>{"content", "files_with_matches", "count"}}
            },
            .keywords = {"search", "grep", "find", "pattern", "regex", "match"}
        },
        grep_handler,
        "builtin"
    );
}

}  // namespace gpagent::tools::builtin
