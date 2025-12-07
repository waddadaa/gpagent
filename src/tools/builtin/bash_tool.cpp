#include "gpagent/tools/tool_registry.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <regex>
#include <sstream>
#include <chrono>

#ifdef __linux__
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#endif

namespace gpagent::tools::builtin {

// Check if command matches blocked patterns
bool is_blocked_command(const std::string& command, const std::vector<std::string>& blocked) {
    for (const auto& pattern : blocked) {
        if (command.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Execute command and capture output
struct CommandResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    bool timed_out = false;
};

CommandResult execute_command(const std::string& command, int timeout_ms,
                               const std::string& working_dir,
                               const std::map<std::string, std::string>& env) {
    CommandResult result;
    result.exit_code = -1;

#ifdef __linux__
    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        result.stderr_output = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        result.stderr_output = "Failed to fork";
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Change directory
        if (!working_dir.empty()) {
            if (chdir(working_dir.c_str()) != 0) {
                _exit(127);
            }
        }

        // Set environment variables
        for (const auto& [key, value] : env) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        // Execute command
        execl("/bin/bash", "bash", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Set up poll for reading
    struct pollfd fds[2];
    fds[0].fd = stdout_pipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = stderr_pipe[0];
    fds[1].events = POLLIN;

    std::ostringstream stdout_ss, stderr_ss;
    std::array<char, 4096> buffer;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        int remaining_ms = timeout_ms - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        if (remaining_ms <= 0) {
            // Timeout - kill the process
            kill(pid, SIGKILL);
            result.timed_out = true;
            break;
        }

        int ret = poll(fds, 2, std::min(remaining_ms, 100));

        if (ret < 0) {
            break;
        }

        if (ret > 0) {
            if (fds[0].revents & POLLIN) {
                ssize_t n = read(stdout_pipe[0], buffer.data(), buffer.size());
                if (n > 0) {
                    stdout_ss.write(buffer.data(), n);
                }
            }
            if (fds[1].revents & POLLIN) {
                ssize_t n = read(stderr_pipe[0], buffer.data(), buffer.size());
                if (n > 0) {
                    stderr_ss.write(buffer.data(), n);
                }
            }

            // Check if both pipes are closed
            if ((fds[0].revents & POLLHUP) && (fds[1].revents & POLLHUP)) {
                break;
            }
        }

        // Check if child has exited
        int status;
        pid_t wpid = waitpid(pid, &status, WNOHANG);
        if (wpid == pid) {
            // Read any remaining data
            ssize_t n;
            while ((n = read(stdout_pipe[0], buffer.data(), buffer.size())) > 0) {
                stdout_ss.write(buffer.data(), n);
            }
            while ((n = read(stderr_pipe[0], buffer.data(), buffer.size())) > 0) {
                stderr_ss.write(buffer.data(), n);
            }

            if (WIFEXITED(status)) {
                result.exit_code = WEXITSTATUS(status);
            }
            break;
        }
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    // Ensure child is reaped
    int status;
    waitpid(pid, &status, 0);

    result.stdout_output = stdout_ss.str();
    result.stderr_output = stderr_ss.str();

#else
    // Fallback for non-Linux (Windows, macOS)
    // Use popen - less control but portable
    std::string full_command = "cd " + working_dir + " && " + command + " 2>&1";

    FILE* pipe = popen(full_command.c_str(), "r");
    if (!pipe) {
        result.stderr_output = "Failed to execute command";
        return result;
    }

    std::array<char, 4096> buffer;
    std::ostringstream output;

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output << buffer.data();
    }

    result.exit_code = pclose(pipe);
    result.stdout_output = output.str();
#endif

    return result;
}

ToolResult bash_handler(const Json& args, const ToolContext& ctx) {
    std::string command = args.at("command").get<std::string>();
    int timeout_ms = args.value("timeout", ctx.timeout_ms);
    std::string description = args.value("description", "");

    // Security checks
    static const std::vector<std::string> default_blocked = {
        "rm -rf /",
        "rm -rf /*",
        "> /dev/sd",
        "dd if=/dev/zero",
        ":(){:|:&};:",  // Fork bomb
        "mkfs",
        "format",
    };

    if (is_blocked_command(command, default_blocked)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Command is blocked for safety reasons"
        };
    }

    // Execute
    auto cmd_result = execute_command(command, timeout_ms, ctx.working_directory, ctx.env);

    if (cmd_result.timed_out) {
        return ToolResult{
            .success = false,
            .content = cmd_result.stdout_output,
            .error_message = "Command timed out after " + std::to_string(timeout_ms) + "ms"
        };
    }

    // Combine output
    std::ostringstream output;
    if (!cmd_result.stdout_output.empty()) {
        output << cmd_result.stdout_output;
    }
    if (!cmd_result.stderr_output.empty()) {
        if (!cmd_result.stdout_output.empty()) {
            output << "\n";
        }
        output << "[stderr]\n" << cmd_result.stderr_output;
    }

    std::string content = output.str();

    // Truncate if too long
    const size_t max_output = 30000;
    if (content.length() > max_output) {
        content = content.substr(0, max_output) + "\n... [output truncated]";
    }

    return ToolResult{
        .success = cmd_result.exit_code == 0,
        .content = content,
        .error_message = cmd_result.exit_code != 0 ?
            std::make_optional("Command exited with code " + std::to_string(cmd_result.exit_code)) :
            std::nullopt
    };
}

void register_bash_tool(ToolRegistry& registry) {
    registry.register_tool(
        ToolSpec{
            .name = "bash",
            .description = "Execute a bash command in the shell. Use for git, npm, docker, and other system commands.",
            .parameters = {
                {"command", "The bash command to execute", ParamType::String, true},
                {"timeout", "Timeout in milliseconds (default: 120000)", ParamType::Integer, false},
                {"description", "Short description of what this command does", ParamType::String, false}
            },
            .keywords = {"bash", "shell", "command", "execute", "run", "terminal", "git", "npm", "docker"},
            .timeout_ms = 120000
        },
        bash_handler,
        "builtin"
    );
}

}  // namespace gpagent::tools::builtin
