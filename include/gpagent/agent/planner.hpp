#pragma once

#include "gpagent/core/result.hpp"
#include "gpagent/core/types.hpp"
#include "gpagent/memory/episodic_memory.hpp"
#include "gpagent/trm/trm_model.hpp"

#include <string>
#include <vector>

namespace gpagent::agent {

using namespace gpagent::core;

// A step in a plan
struct PlanStep {
    std::string description;
    std::string suggested_tool;
    float confidence = 0.0f;
    bool completed = false;
    std::string result;
};

// A task plan
struct Plan {
    std::string task;
    std::vector<PlanStep> steps;
    TimePoint created_at;
    bool is_complete() const;
    int completed_count() const;
};

// Planner - creates high-level plans for tasks
// Uses TRM predictions and past episodes to suggest tool sequences
class Planner {
public:
    explicit Planner(trm::TRMModel& trm);

    // Create a plan for a task
    Plan create_plan(
        const std::string& task,
        const std::vector<std::string>& available_tools,
        const std::vector<memory::Episode>& relevant_episodes = {}
    );

    // Update plan based on execution results
    void update_plan(
        Plan& plan,
        int step_index,
        bool success,
        const std::string& result
    );

    // Get next suggested step
    std::optional<PlanStep> next_step(const Plan& plan) const;

    // Check if plan seems stuck (too many failures)
    bool is_stuck(const Plan& plan, int max_failures = 3) const;

    // Replan after failures
    Plan replan(
        const Plan& original,
        const std::string& failure_reason,
        const std::vector<std::string>& available_tools
    );

private:
    trm::TRMModel& trm_;

    // Extract tool suggestions from task description
    std::vector<std::string> extract_tool_hints(const std::string& task) const;

    // Learn from past episodes to improve planning
    std::vector<PlanStep> learn_from_episodes(
        const std::string& task,
        const std::vector<memory::Episode>& episodes
    ) const;
};

}  // namespace gpagent::agent
