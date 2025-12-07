#include "gpagent/tools/tool_registry.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace gpagent::tools::builtin {

namespace fs = std::filesystem;

// Execute a command and capture output with timeout
std::pair<int, std::string> exec_with_timeout(const std::string& cmd, int timeout_sec) {
    std::string full_cmd = "timeout " + std::to_string(timeout_sec) + " " + cmd + " 2>&1";

    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        return {-1, "Failed to execute command"};
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
        // Limit output size
        if (result.size() > 100000) {
            result += "\n... [output truncated]";
            break;
        }
    }

    int status = pclose(pipe);
    int exit_code = WEXITSTATUS(status);

    // Check for timeout (exit code 124)
    if (exit_code == 124) {
        result += "\n[Execution timed out after " + std::to_string(timeout_sec) + " seconds]";
    }

    return {exit_code, result};
}

ToolResult code_execute_python_handler(const Json& args, const ToolContext& ctx) {
    std::string code = args.at("code").get<std::string>();
    int timeout = args.value("timeout", 30);

    // Create a temporary file for the code
    fs::path temp_dir = fs::temp_directory_path();
    fs::path script_path = temp_dir / ("gpagent_py_" + std::to_string(std::time(nullptr)) + ".py");

    try {
        // Write code to temp file
        std::ofstream script(script_path);
        if (!script) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to create temporary script file"
            };
        }
        script << code;
        script.close();

        // Execute with Python
        std::string cmd = "python3 " + script_path.string();
        auto [exit_code, output] = exec_with_timeout(cmd, timeout);

        // Clean up
        fs::remove(script_path);

        if (exit_code != 0 && exit_code != 124) {
            return ToolResult{
                .success = false,
                .content = output,
                .error_message = "Python execution failed with exit code " + std::to_string(exit_code)
            };
        }

        return ToolResult{
            .success = (exit_code == 0),
            .content = output.empty() ? "(no output)" : output
        };

    } catch (const std::exception& e) {
        // Clean up on error
        if (fs::exists(script_path)) {
            fs::remove(script_path);
        }
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error executing Python: ") + e.what()
        };
    }
}

ToolResult code_execute_javascript_handler(const Json& args, const ToolContext& ctx) {
    std::string code = args.at("code").get<std::string>();
    int timeout = args.value("timeout", 30);

    // Create a temporary file for the code
    fs::path temp_dir = fs::temp_directory_path();
    fs::path script_path = temp_dir / ("gpagent_js_" + std::to_string(std::time(nullptr)) + ".js");

    try {
        // Write code to temp file
        std::ofstream script(script_path);
        if (!script) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "Failed to create temporary script file"
            };
        }
        script << code;
        script.close();

        // Try node first, then deno
        std::string cmd = "node " + script_path.string();
        auto [exit_code, output] = exec_with_timeout(cmd, timeout);

        // If node not found, try deno
        if (output.find("command not found") != std::string::npos ||
            output.find("not found") != std::string::npos) {
            cmd = "deno run " + script_path.string();
            std::tie(exit_code, output) = exec_with_timeout(cmd, timeout);
        }

        // Clean up
        fs::remove(script_path);

        if (exit_code != 0 && exit_code != 124) {
            return ToolResult{
                .success = false,
                .content = output,
                .error_message = "JavaScript execution failed with exit code " + std::to_string(exit_code)
            };
        }

        return ToolResult{
            .success = (exit_code == 0),
            .content = output.empty() ? "(no output)" : output
        };

    } catch (const std::exception& e) {
        // Clean up on error
        if (fs::exists(script_path)) {
            fs::remove(script_path);
        }
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = std::string("Error executing JavaScript: ") + e.what()
        };
    }
}

ToolResult code_execute_handler(const Json& args, const ToolContext& ctx) {
    std::string code = args.at("code").get<std::string>();
    std::string language = args.value("language", "python");

    // Normalize language name
    if (language == "py" || language == "python3") {
        language = "python";
    } else if (language == "js" || language == "node") {
        language = "javascript";
    }

    if (language == "python") {
        return code_execute_python_handler(args, ctx);
    } else if (language == "javascript") {
        return code_execute_javascript_handler(args, ctx);
    } else {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Unsupported language: " + language + ". Supported: python, javascript"
        };
    }
}

// Register code execution tools
void register_code_tools(ToolRegistry& registry) {
    registry.register_tool(
        ToolSpec{
            .name = "code_execute",
            .description = "Execute code in a sandboxed environment. Supports Python and JavaScript.",
            .parameters = {
                {"code", "The code to execute", ParamType::String, true},
                {"language", "Programming language: python, javascript (default: python)", ParamType::String, false},
                {"timeout", "Execution timeout in seconds (default: 30)", ParamType::Integer, false}
            },
            .keywords = {"execute", "run", "code", "python", "javascript", "eval"},
            .requires_confirmation = true,
            .timeout_ms = 60000
        },
        code_execute_handler,
        "builtin"
    );
}

}  // namespace gpagent::tools::builtin
