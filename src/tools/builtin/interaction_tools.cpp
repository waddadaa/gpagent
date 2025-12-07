#include "gpagent/tools/tool_registry.hpp"

#include <iostream>

namespace gpagent::tools::builtin {

ToolResult ask_user_handler(const Json& args, const ToolContext& ctx) {
    (void)ctx;

    std::string question = args.at("question").get<std::string>();
    std::vector<std::string> options;

    if (args.contains("options") && args["options"].is_array()) {
        for (const auto& opt : args["options"]) {
            options.push_back(opt.get<std::string>());
        }
    }

    // Display the question
    std::cout << "\n\033[1;33m[Question]\033[0m " << question << "\n";

    if (!options.empty()) {
        std::cout << "Options:\n";
        for (size_t i = 0; i < options.size(); ++i) {
            std::cout << "  " << (i + 1) << ") " << options[i] << "\n";
        }
        std::cout << "Enter choice (number or text): ";
    } else {
        std::cout << "Your answer: ";
    }

    std::cout.flush();

    std::string response;
    if (!std::getline(std::cin, response)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Failed to read user input"
        };
    }

    // If options provided and user entered a number, map to option text
    if (!options.empty() && !response.empty()) {
        try {
            int choice = std::stoi(response);
            if (choice >= 1 && choice <= static_cast<int>(options.size())) {
                response = options[choice - 1];
            }
        } catch (...) {
            // Not a number, use the text as-is
        }
    }

    return ToolResult{
        .success = true,
        .content = response
    };
}

ToolResult task_complete_handler(const Json& args, const ToolContext& ctx) {
    (void)ctx;

    std::string summary = args.value("summary", "Task completed");
    bool success = args.value("success", true);

    std::string status = success ? "\033[1;32m[SUCCESS]\033[0m" : "\033[1;31m[FAILED]\033[0m";
    std::cout << "\n" << status << " " << summary << "\n";

    return ToolResult{
        .success = true,
        .content = success ? "Task marked as complete" : "Task marked as failed"
    };
}

ToolResult notify_user_handler(const Json& args, const ToolContext& ctx) {
    (void)ctx;

    std::string message = args.at("message").get<std::string>();
    std::string level = args.value("level", "info");

    std::string prefix;
    if (level == "error") {
        prefix = "\033[1;31m[ERROR]\033[0m";
    } else if (level == "warning") {
        prefix = "\033[1;33m[WARNING]\033[0m";
    } else if (level == "success") {
        prefix = "\033[1;32m[SUCCESS]\033[0m";
    } else {
        prefix = "\033[1;34m[INFO]\033[0m";
    }

    std::cout << prefix << " " << message << "\n";

    return ToolResult{
        .success = true,
        .content = "User notified"
    };
}

ToolResult confirm_action_handler(const Json& args, const ToolContext& ctx) {
    (void)ctx;

    std::string action = args.at("action").get<std::string>();
    std::string details = args.value("details", "");

    std::cout << "\n\033[1;33m[Confirmation Required]\033[0m\n";
    std::cout << "Action: " << action << "\n";
    if (!details.empty()) {
        std::cout << "Details: " << details << "\n";
    }
    std::cout << "Proceed? (y/n): ";
    std::cout.flush();

    std::string response;
    if (!std::getline(std::cin, response)) {
        return ToolResult{
            .success = false,
            .content = "",
            .error_message = "Failed to read user input"
        };
    }

    // Normalize response
    bool confirmed = !response.empty() &&
                     (response[0] == 'y' || response[0] == 'Y');

    return ToolResult{
        .success = true,
        .content = confirmed ? "confirmed" : "denied"
    };
}

// Register interaction tools
void register_interaction_tools(ToolRegistry& registry) {
    registry.register_tool(
        ToolSpec{
            .name = "ask_user",
            .description = "Ask the user a question and wait for their response.",
            .parameters = {
                {"question", "The question to ask", ParamType::String, true},
                {"options", "Optional list of choices to present", ParamType::Array, false}
            },
            .keywords = {"ask", "question", "input", "user", "prompt"}
        },
        ask_user_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "task_complete",
            .description = "Mark the current task as complete and provide a summary.",
            .parameters = {
                {"summary", "Summary of what was accomplished", ParamType::String, false},
                {"success", "Whether the task was successful (default: true)", ParamType::Boolean, false}
            },
            .keywords = {"done", "complete", "finish", "task", "end"}
        },
        task_complete_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "notify_user",
            .description = "Display a notification message to the user.",
            .parameters = {
                {"message", "The message to display", ParamType::String, true},
                {"level", "Notification level: info, warning, error, success (default: info)", ParamType::String, false}
            },
            .keywords = {"notify", "message", "alert", "show"}
        },
        notify_user_handler,
        "builtin"
    );

    registry.register_tool(
        ToolSpec{
            .name = "confirm_action",
            .description = "Ask the user to confirm an action before proceeding.",
            .parameters = {
                {"action", "Description of the action requiring confirmation", ParamType::String, true},
                {"details", "Additional details about the action", ParamType::String, false}
            },
            .keywords = {"confirm", "approve", "verify", "check"}
        },
        confirm_action_handler,
        "builtin"
    );
}

}  // namespace gpagent::tools::builtin
