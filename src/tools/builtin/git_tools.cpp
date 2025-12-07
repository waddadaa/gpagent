#include "gpagent/tools/tool_registry.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <sstream>

namespace gpagent::tools::builtin {

namespace fs = std::filesystem;

// Execute a command and capture output
std::pair<int, std::string> exec_command(const std::string& cmd, const std::string& cwd = "") {
    std::string full_cmd = cmd;
    if (!cwd.empty()) {
        full_cmd = "cd " + cwd + " && " + cmd;
    }
    full_cmd += " 2>&1";

    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        return {-1, "Failed to execute command"};
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);
    int exit_code = WEXITSTATUS(status);

    return {exit_code, result};
}

// Check if path is a git repository
bool is_git_repo(const std::string& path) {
    fs::path git_dir = fs::path(path) / ".git";
    return fs::exists(git_dir);
}

ToolResult git_status_handler(const Json& args, const ToolContext& ctx) {
    std::string repo_path = args.value("path", ctx.working_directory);

    if (!is_git_repo(repo_path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Not a git repository: " + repo_path
        };
    }

    auto [exit_code, output] = exec_command("git status", repo_path);

    if (exit_code != 0) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "git status failed: " + output
        };
    }

    return ToolResult{
        .success = true,
        .content = output
    };
}

ToolResult git_diff_handler(const Json& args, const ToolContext& ctx) {
    std::string repo_path = args.value("path", ctx.working_directory);
    bool staged = args.value("staged", false);
    std::string file_path = args.value("file", "");

    if (!is_git_repo(repo_path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Not a git repository: " + repo_path
        };
    }

    std::string cmd = "git diff";
    if (staged) {
        cmd += " --staged";
    }
    if (!file_path.empty()) {
        cmd += " -- " + file_path;
    }

    auto [exit_code, output] = exec_command(cmd, repo_path);

    if (exit_code != 0) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "git diff failed: " + output
        };
    }

    if (output.empty()) {
        return ToolResult{
            .success = true,
            .content = "No changes"
        };
    }

    return ToolResult{
        .success = true,
        .content = output
    };
}

ToolResult git_log_handler(const Json& args, const ToolContext& ctx) {
    std::string repo_path = args.value("path", ctx.working_directory);
    int num_commits = args.value("num_commits", 10);
    bool oneline = args.value("oneline", true);

    if (!is_git_repo(repo_path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Not a git repository: " + repo_path
        };
    }

    std::string cmd = "git log -n " + std::to_string(num_commits);
    if (oneline) {
        cmd += " --oneline";
    } else {
        cmd += " --format='%h %ad | %s [%an]' --date=short";
    }

    auto [exit_code, output] = exec_command(cmd, repo_path);

    if (exit_code != 0) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "git log failed: " + output
        };
    }

    return ToolResult{
        .success = true,
        .content = output
    };
}

ToolResult git_commit_handler(const Json& args, const ToolContext& ctx) {
    std::string repo_path = args.value("path", ctx.working_directory);
    std::string message = args.at("message").get<std::string>();
    bool add_all = args.value("add_all", false);
    std::vector<std::string> files;

    if (args.contains("files") && args["files"].is_array()) {
        for (const auto& f : args["files"]) {
            files.push_back(f.get<std::string>());
        }
    }

    if (!is_git_repo(repo_path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Not a git repository: " + repo_path
        };
    }

    // Stage files
    if (add_all) {
        auto [exit_code, output] = exec_command("git add -A", repo_path);
        if (exit_code != 0) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "git add failed: " + output
            };
        }
    } else if (!files.empty()) {
        std::string add_cmd = "git add";
        for (const auto& f : files) {
            add_cmd += " '" + f + "'";
        }
        auto [exit_code, output] = exec_command(add_cmd, repo_path);
        if (exit_code != 0) {
            return ToolResult{
                .success = false,
                .content = "",
                .error_message = "git add failed: " + output
            };
        }
    }

    // Commit
    std::string commit_cmd = "git commit -m '" + message + "'";
    auto [exit_code, output] = exec_command(commit_cmd, repo_path);

    if (exit_code != 0) {
        // Check if nothing to commit
        if (output.find("nothing to commit") != std::string::npos) {
            return ToolResult{
                .success = true,
                .content = "Nothing to commit, working tree clean"
            };
        }
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "git commit failed: " + output
        };
    }

    return ToolResult{
        .success = true,
        .content = output
    };
}

ToolResult git_branch_handler(const Json& args, const ToolContext& ctx) {
    std::string repo_path = args.value("path", ctx.working_directory);
    bool show_all = args.value("all", false);

    if (!is_git_repo(repo_path)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Not a git repository: " + repo_path
        };
    }

    std::string cmd = "git branch";
    if (show_all) {
        cmd += " -a";
    }

    auto [exit_code, output] = exec_command(cmd, repo_path);

    if (exit_code != 0) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "git branch failed: " + output
        };
    }

    return ToolResult{
        .success = true,
        .content = output
    };
}

// Register git tools
void register_git_tools(ToolRegistry& registry) {
    registry.register_tool(
        ToolSpec{
            .name = "git_status",
            .description = "Show the working tree status of a git repository.",
            .parameters = {
                {"path", "Path to the git repository (default: working directory)", ParamType::String, false}
            },
            .keywords = {"git", "status", "changes", "modified", "staged"}
        },
        git_status_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "git_diff",
            .description = "Show changes between commits, commit and working tree, etc.",
            .parameters = {
                {"path", "Path to the git repository (default: working directory)", ParamType::String, false},
                {"staged", "Show staged changes only (default: false)", ParamType::Boolean, false},
                {"file", "Show diff for a specific file only", ParamType::String, false}
            },
            .keywords = {"git", "diff", "changes", "compare"}
        },
        git_diff_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "git_log",
            .description = "Show commit logs.",
            .parameters = {
                {"path", "Path to the git repository (default: working directory)", ParamType::String, false},
                {"num_commits", "Number of commits to show (default: 10)", ParamType::Integer, false},
                {"oneline", "Show each commit on one line (default: true)", ParamType::Boolean, false}
            },
            .keywords = {"git", "log", "history", "commits"}
        },
        git_log_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "git_commit",
            .description = "Record changes to the repository. Can stage files before committing.",
            .parameters = {
                {"message", "Commit message", ParamType::String, true},
                {"path", "Path to the git repository (default: working directory)", ParamType::String, false},
                {"add_all", "Stage all changes before commit (default: false)", ParamType::Boolean, false},
                {"files", "Specific files to stage before commit", ParamType::Array, false}
            },
            .keywords = {"git", "commit", "save", "record"},
            .requires_confirmation = true
        },
        git_commit_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "git_branch",
            .description = "List branches in the repository.",
            .parameters = {
                {"path", "Path to the git repository (default: working directory)", ParamType::String, false},
                {"all", "Show remote branches too (default: false)", ParamType::Boolean, false}
            },
            .keywords = {"git", "branch", "branches"}
        },
        git_branch_handler,
        "builtin"
    );
}

}  // namespace gpagent::tools::builtin
