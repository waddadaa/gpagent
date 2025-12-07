#include "gpagent/agent/planner.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace gpagent::agent {

bool Plan::is_complete() const {
    return std::all_of(steps.begin(), steps.end(),
                       [](const PlanStep& s) { return s.completed; });
}

int Plan::completed_count() const {
    return static_cast<int>(std::count_if(steps.begin(), steps.end(),
                                           [](const PlanStep& s) { return s.completed; }));
}

Planner::Planner(trm::TRMModel& trm)
    : trm_(trm)
{
}

Plan Planner::create_plan(
    const std::string& task,
    const std::vector<std::string>& available_tools,
    const std::vector<memory::Episode>& relevant_episodes) {

    Plan plan;
    plan.task = task;
    plan.created_at = Clock::now();

    // First, try to learn from past episodes
    if (!relevant_episodes.empty()) {
        auto learned_steps = learn_from_episodes(task, relevant_episodes);
        if (!learned_steps.empty()) {
            plan.steps = std::move(learned_steps);
            return plan;
        }
    }

    // Extract tool hints from task description
    auto hints = extract_tool_hints(task);

    // Use TRM to predict tool sequence
    if (trm_.is_ready()) {
        auto prediction = trm_.predict(task, available_tools);
        if (prediction) {
            // Create steps based on TRM predictions
            for (const auto& [tool, score] : prediction->ranked_tools) {
                if (score < 0.1f) break;  // Skip very low confidence

                PlanStep step;
                step.suggested_tool = tool;
                step.confidence = score;

                // Generate description based on tool
                if (tool == "file_read") {
                    step.description = "Read relevant files to understand the task";
                } else if (tool == "grep" || tool == "glob") {
                    step.description = "Search for relevant code or files";
                } else if (tool == "file_edit") {
                    step.description = "Make necessary code changes";
                } else if (tool == "file_write") {
                    step.description = "Create new files as needed";
                } else if (tool == "bash") {
                    step.description = "Run commands to verify or build";
                } else {
                    step.description = "Use " + tool + " for the task";
                }

                plan.steps.push_back(std::move(step));
            }
        }
    }

    // If no TRM or no prediction, use heuristic steps
    if (plan.steps.empty()) {
        // Default plan structure: understand -> modify -> verify
        plan.steps.push_back({
            "Understand the codebase and task requirements",
            "file_read",
            0.7f,
            false,
            ""
        });

        if (!hints.empty()) {
            for (const auto& hint : hints) {
                plan.steps.push_back({
                    "Execute task using " + hint,
                    hint,
                    0.5f,
                    false,
                    ""
                });
            }
        } else {
            plan.steps.push_back({
                "Make necessary changes",
                "file_edit",
                0.5f,
                false,
                ""
            });
        }

        plan.steps.push_back({
            "Verify changes work correctly",
            "bash",
            0.6f,
            false,
            ""
        });
    }

    return plan;
}

void Planner::update_plan(
    Plan& plan,
    int step_index,
    bool success,
    const std::string& result) {

    if (step_index < 0 || step_index >= static_cast<int>(plan.steps.size())) {
        return;
    }

    plan.steps[step_index].completed = true;
    plan.steps[step_index].result = result;

    // If step failed, adjust confidence of subsequent similar steps
    if (!success) {
        const auto& failed_tool = plan.steps[step_index].suggested_tool;
        for (size_t i = step_index + 1; i < plan.steps.size(); ++i) {
            if (plan.steps[i].suggested_tool == failed_tool) {
                plan.steps[i].confidence *= 0.5f;  // Reduce confidence
            }
        }
    }
}

std::optional<PlanStep> Planner::next_step(const Plan& plan) const {
    for (const auto& step : plan.steps) {
        if (!step.completed) {
            return step;
        }
    }
    return std::nullopt;
}

bool Planner::is_stuck(const Plan& plan, int max_failures) const {
    int consecutive_failures = 0;

    for (const auto& step : plan.steps) {
        if (step.completed) {
            // Check if result indicates failure
            bool failed = step.result.find("error") != std::string::npos ||
                          step.result.find("failed") != std::string::npos ||
                          step.result.find("Error") != std::string::npos;

            if (failed) {
                ++consecutive_failures;
                if (consecutive_failures >= max_failures) {
                    return true;
                }
            } else {
                consecutive_failures = 0;
            }
        }
    }

    return false;
}

Plan Planner::replan(
    const Plan& original,
    const std::string& failure_reason,
    const std::vector<std::string>& available_tools) {

    Plan new_plan;
    new_plan.task = original.task + " (replanned after: " + failure_reason + ")";
    new_plan.created_at = Clock::now();

    // Keep completed successful steps
    for (const auto& step : original.steps) {
        if (step.completed &&
            step.result.find("error") == std::string::npos &&
            step.result.find("failed") == std::string::npos) {
            new_plan.steps.push_back(step);
        }
    }

    // Add recovery step
    PlanStep recovery;
    recovery.description = "Recover from: " + failure_reason;
    recovery.suggested_tool = "file_read";  // Usually need to understand what went wrong
    recovery.confidence = 0.6f;
    new_plan.steps.push_back(recovery);

    // Find uncompleted steps and add alternatives
    for (const auto& step : original.steps) {
        if (!step.completed) {
            // Try to find alternative tool
            std::string alt_tool = step.suggested_tool;

            // Simple tool alternatives
            static const std::unordered_map<std::string, std::string> alternatives = {
                {"file_edit", "file_write"},
                {"grep", "glob"},
                {"bash", "file_read"},
            };

            auto it = alternatives.find(step.suggested_tool);
            if (it != alternatives.end()) {
                alt_tool = it->second;
            }

            PlanStep new_step = step;
            new_step.suggested_tool = alt_tool;
            new_step.confidence *= 0.8f;  // Lower confidence for alternative
            new_plan.steps.push_back(new_step);
        }
    }

    return new_plan;
}

std::vector<std::string> Planner::extract_tool_hints(const std::string& task) const {
    std::vector<std::string> hints;
    std::string lower_task = task;
    std::transform(lower_task.begin(), lower_task.end(), lower_task.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Keywords that suggest specific tools
    static const std::vector<std::pair<std::vector<std::string>, std::string>> keyword_tools = {
        {{"read", "show", "display", "view", "cat", "look at"}, "file_read"},
        {{"write", "create", "new file", "generate"}, "file_write"},
        {{"edit", "modify", "change", "update", "fix", "refactor"}, "file_edit"},
        {{"search", "find", "grep", "locate", "where"}, "grep"},
        {{"list", "files", "directory", "ls"}, "glob"},
        {{"run", "execute", "build", "test", "compile", "install"}, "bash"},
    };

    for (const auto& [keywords, tool] : keyword_tools) {
        for (const auto& keyword : keywords) {
            if (lower_task.find(keyword) != std::string::npos) {
                // Avoid duplicates
                if (std::find(hints.begin(), hints.end(), tool) == hints.end()) {
                    hints.push_back(tool);
                }
                break;
            }
        }
    }

    return hints;
}

std::vector<PlanStep> Planner::learn_from_episodes(
    const std::string& task,
    const std::vector<memory::Episode>& episodes) const {

    std::vector<PlanStep> steps;

    // Find the most similar successful episode
    const memory::Episode* best_match = nullptr;
    float best_similarity = 0.0f;

    for (const auto& ep : episodes) {
        if (!ep.outcome.success) continue;

        // Simple word overlap similarity
        std::unordered_set<std::string> task_words;
        std::istringstream iss1(task);
        std::string word;
        while (iss1 >> word) {
            std::transform(word.begin(), word.end(), word.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            task_words.insert(word);
        }

        std::unordered_set<std::string> ep_words;
        std::istringstream iss2(ep.task_description);
        while (iss2 >> word) {
            std::transform(word.begin(), word.end(), word.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            ep_words.insert(word);
        }

        int overlap = 0;
        for (const auto& w : task_words) {
            if (ep_words.count(w)) ++overlap;
        }

        float similarity = static_cast<float>(overlap) /
                           static_cast<float>(std::max(task_words.size(), ep_words.size()));

        if (similarity > best_similarity) {
            best_similarity = similarity;
            best_match = &ep;
        }
    }

    // If we found a good match, use its action sequence
    if (best_match && best_similarity > 0.3f) {
        for (const auto& action : best_match->actions) {
            if (!action.success) continue;  // Skip failed actions

            PlanStep step;
            step.suggested_tool = action.tool;
            step.description = "Use " + action.tool + " (from similar task)";
            step.confidence = best_similarity * (action.success ? 0.8f : 0.3f);

            steps.push_back(std::move(step));
        }
    }

    return steps;
}

}  // namespace gpagent::agent
