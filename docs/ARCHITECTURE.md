# KeneticCore: C++ Agent Platform Architecture

**Version:** 0.2.0
**Last Updated:** 2025-12-05

## Overview

KeneticCore is a high-performance C++ agent platform featuring:
- **TRM-based RL** for continuous agent improvement
- **File-based hierarchical memory** (Claude Code style)
- **MCP-compatible tool system**
- **Multi-LLM support** (Claude Opus 4.5, Gemini 3 Pro)
- **Context compaction** for long-running agents
- **Progressive learning** - LLM-first with automatic TRM training kickoff
- **Full configurability** - All defaults can be overridden via config

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Directory Structure](#directory-structure)
3. [Core Foundation](#core-foundation)
4. [Memory System](#1-memory-system)
5. [TRM Reasoning Engine](#2-trm-reasoning-engine)
6. [Tool System](#3-tool-system)
7. [Context Management](#4-context-management)
8. [LLM Gateway](#5-llm-gateway)
9. [Agent Orchestrator](#6-agent-orchestrator)
10. [Reward & Training System](#7-reward--training-system)
11. [Configuration System](#8-configuration-system)
12. [Error Handling](#9-error-handling)
13. [Concurrency Model](#10-concurrency-model)
14. [Security](#11-security)
15. [Observability](#12-observability)
16. [Build & Dependencies](#build--dependencies)

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              KENETIC CORE PLATFORM                               │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ┌────────────────────────────────────────────────────────────────────────────┐ │
│  │                           AGENT ORCHESTRATOR                                │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │ │
│  │  │   Planner    │  │   Executor   │  │  Evaluator   │  │   Router     │   │ │
│  │  │  (ReAct/CoT) │  │ (Tool Call)  │  │ (RL Reward)  │  │ (Multi-Agent)│   │ │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │ │
│  │         │                 │                 │                 │           │ │
│  │         └─────────────────┴─────────────────┴─────────────────┘           │ │
│  │                                    │                                       │ │
│  └────────────────────────────────────┼───────────────────────────────────────┘ │
│                                       │                                          │
│  ┌────────────────────────────────────┼───────────────────────────────────────┐ │
│  │                          CORE SERVICES LAYER                               │ │
│  │                                    │                                       │ │
│  │  ┌─────────────┐  ┌─────────────┐  │  ┌─────────────┐  ┌─────────────┐    │ │
│  │  │   Memory    │  │   Context   │◄─┴─►│    Tool     │  │     LLM     │    │ │
│  │  │   Manager   │  │   Manager   │     │   Registry  │  │   Gateway   │    │ │
│  │  └──────┬──────┘  └──────┬──────┘     └──────┬──────┘  └──────┬──────┘    │ │
│  │         │                │                   │                │           │ │
│  └─────────┼────────────────┼───────────────────┼────────────────┼───────────┘ │
│            │                │                   │                │             │
│  ┌─────────┼────────────────┼───────────────────┼────────────────┼───────────┐ │
│  │         │          TRM REASONING ENGINE      │                │           │ │
│  │         │                │                   │                │           │ │
│  │  ┌──────▼──────┐  ┌──────▼──────┐  ┌────────▼────────┐  ┌────▼────────┐  │ │
│  │  │   Episodic  │  │   Context   │  │  Tool Selection │  │   Policy    │  │ │
│  │  │   Learner   │  │  Compactor  │  │    Optimizer    │  │   Network   │  │ │
│  │  │   (7M TRM)  │  │  (Summarize)│  │    (7M TRM)     │  │  (7M TRM)   │  │ │
│  │  └─────────────┘  └─────────────┘  └─────────────────┘  └─────────────┘  │ │
│  │                                                                          │ │
│  └──────────────────────────────────────────────────────────────────────────┘ │
│                                                                                │
│  ┌──────────────────────────────────────────────────────────────────────────┐ │
│  │                           STORAGE LAYER                                   │ │
│  │                                                                           │ │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐           │ │
│  │  │  File Storage   │  │  Checkpoint DB  │  │  Vector Store   │           │ │
│  │  │  (JSON/JSONL)   │  │    (SQLite)     │  │ (FAISS/optional)│           │ │
│  │  └─────────────────┘  └─────────────────┘  └─────────────────┘           │ │
│  │                                                                           │ │
│  └──────────────────────────────────────────────────────────────────────────┘ │
│                                                                                │
└─────────────────────────────────────────────────────────────────────────────────┘
                                       │
                    ┌──────────────────┼──────────────────┐
                    │                  │                  │
              ┌─────▼─────┐     ┌──────▼──────┐    ┌─────▼─────┐
              │  Claude   │     │   Gemini    │    │  Local    │
              │ Opus 4.5  │     │   3 Pro     │    │  (llama)  │
              └───────────┘     └─────────────┘    └───────────┘
```

---

## Directory Structure

```
keneticCore/
├── CMakeLists.txt
├── README.md
├── docs/
│   └── ARCHITECTURE.md
│
├── include/
│   └── kenetic/
│       ├── core/
│       │   ├── types.hpp              # Common types and aliases
│       │   ├── config.hpp             # Configuration management
│       │   ├── errors.hpp             # Error codes and error handling
│       │   ├── result.hpp             # Result<T,E> monadic type
│       │   └── uuid.hpp               # UUID generation utilities
│       │
│       ├── agent/
│       │   ├── agent.hpp              # Base agent interface
│       │   ├── orchestrator.hpp       # Multi-agent orchestration
│       │   ├── planner.hpp            # Planning module (ReAct/CoT)
│       │   ├── executor.hpp           # Tool execution
│       │   └── evaluator.hpp          # Reward computation
│       │
│       ├── memory/
│       │   ├── memory_manager.hpp     # Main memory interface
│       │   ├── session_state.hpp      # Short-term session memory
│       │   ├── thread_memory.hpp      # Conversation thread storage
│       │   ├── cross_thread.hpp       # Cross-session memory
│       │   ├── episodic_memory.hpp    # Experience storage
│       │   ├── checkpointer.hpp       # State checkpointing
│       │   └── compactor.hpp          # Context compression
│       │
│       ├── tools/
│       │   ├── tool_registry.hpp      # Tool management
│       │   ├── tool_spec.hpp          # Tool definition schema
│       │   ├── mcp_server.hpp         # MCP protocol server
│       │   └── builtin/               # Built-in tools
│       │       ├── file_tools.hpp
│       │       ├── bash_tool.hpp
│       │       ├── search_tools.hpp
│       │       └── web_tools.hpp
│       │
│       ├── llm/
│       │   ├── llm_gateway.hpp        # Multi-LLM interface
│       │   ├── providers/
│       │   │   ├── claude.hpp         # Anthropic Claude
│       │   │   ├── gemini.hpp         # Google Gemini
│       │   │   └── local.hpp          # llama.cpp local
│       │   ├── tokenizer.hpp          # Token counting
│       │   └── message.hpp            # Message types
│       │
│       ├── trm/
│       │   ├── trm_model.hpp          # TRM network definition
│       │   ├── trm_reasoner.hpp       # Recursive reasoning
│       │   ├── trm_trainer.hpp        # Training loop
│       │   └── reward_model.hpp       # Reward computation
│       │
│       └── context/
│           ├── context_manager.hpp    # Context window management
│           ├── context_builder.hpp    # Prompt construction
│           └── context_editor.hpp     # Stale content removal
│
├── src/
│   ├── agent/
│   ├── memory/
│   ├── tools/
│   ├── llm/
│   ├── trm/
│   └── context/
│
├── tests/
│   ├── unit/
│   └── integration/
│
├── data/
│   ├── trm_weights/                   # Pre-trained TRM models
│   └── embeddings/                    # Embedding models
│
└── storage/                           # Runtime storage (gitignored)
    ├── sessions/
    ├── checkpoints/
    ├── episodic/
    └── cross_thread/
```

---

## Core Foundation

### Error Types and Result Handling

KeneticCore uses a monadic `Result<T, E>` type for error handling, avoiding exceptions in hot paths.

#### Error Codes

```cpp
namespace kenetic::core {

enum class ErrorCode {
    // Success
    Ok = 0,

    // General errors (1-99)
    Unknown = 1,
    InvalidArgument = 2,
    NotFound = 3,
    AlreadyExists = 4,
    PermissionDenied = 5,
    Timeout = 6,
    Cancelled = 7,

    // Memory errors (100-199)
    MemoryLoadFailed = 100,
    MemorySaveFailed = 101,
    MemoryCorrupted = 102,
    CheckpointNotFound = 103,
    EpisodeNotFound = 104,
    SessionExpired = 105,

    // LLM errors (200-299)
    LLMConnectionFailed = 200,
    LLMRateLimited = 201,
    LLMContextOverflow = 202,
    LLMInvalidResponse = 203,
    LLMApiKeyMissing = 204,
    LLMProviderUnavailable = 205,
    LLMTokenLimitExceeded = 206,

    // Tool errors (300-399)
    ToolNotFound = 300,
    ToolExecutionFailed = 301,
    ToolValidationFailed = 302,
    ToolTimeout = 303,
    ToolPermissionDenied = 304,
    MCPConnectionFailed = 305,
    MCPProtocolError = 306,

    // TRM errors (400-499)
    TRMModelNotLoaded = 400,
    TRMInferenceFailed = 401,
    TRMTrainingFailed = 402,
    TRMInsufficientData = 403,

    // Context errors (500-599)
    ContextBuildFailed = 500,
    ContextCompactionFailed = 501,
    ContextTooLarge = 502,

    // Configuration errors (600-699)
    ConfigNotFound = 600,
    ConfigParseFailed = 601,
    ConfigValidationFailed = 602,

    // File system errors (700-799)
    FileNotFound = 700,
    FileReadFailed = 701,
    FileWriteFailed = 702,
    DirectoryNotFound = 703,
    PathNotAllowed = 704,
};

struct Error {
    ErrorCode code;
    std::string message;
    std::optional<std::string> context;  // Additional context (file path, tool name, etc.)
    std::optional<std::string> stack_trace;

    static Error from_code(ErrorCode code);
    static Error from_exception(const std::exception& e);

    bool is_retriable() const;
    bool is_fatal() const;
};

}  // namespace kenetic::core
```

#### Result Type

```cpp
namespace kenetic::core {

template<typename T, typename E = Error>
class Result {
public:
    // Constructors
    static Result<T, E> ok(T value);
    static Result<T, E> err(E error);

    // Accessors
    bool is_ok() const;
    bool is_err() const;
    T& value();
    const T& value() const;
    E& error();
    const E& error() const;

    // Monadic operations
    template<typename U>
    Result<U, E> map(std::function<U(T)> f) const;

    template<typename U>
    Result<U, E> and_then(std::function<Result<U, E>(T)> f) const;

    Result<T, E> or_else(std::function<Result<T, E>(E)> f) const;

    T unwrap_or(T default_value) const;
    T unwrap_or_else(std::function<T(E)> f) const;

    // Throwing accessor (use sparingly)
    T unwrap() const;  // Throws if error

private:
    std::variant<T, E> data_;
};

// Convenience aliases
template<typename T>
using KResult = Result<T, Error>;

}  // namespace kenetic::core
```

#### UUID Generation

```cpp
namespace kenetic::core {

class UUID {
public:
    static UUID generate();  // UUIDv4 random
    static UUID from_string(const std::string& str);

    std::string to_string() const;
    bool is_valid() const;

    bool operator==(const UUID& other) const;
    bool operator<(const UUID& other) const;

private:
    std::array<uint8_t, 16> bytes_;
};

// Convenience functions for prefixed IDs
std::string generate_session_id();   // "sess_xxxxxxxx"
std::string generate_episode_id();   // "ep_xxxxxxxx"
std::string generate_checkpoint_id(); // "cp_xxxxxxxx"
std::string generate_thread_id();    // "thread_xxxxxxxx"

}  // namespace kenetic::core
```

---

## Component Specifications

### 1. Memory System

#### 1.1 Three-Tier Memory Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                      MEMORY HIERARCHY                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  TIER 1: SHORT-TERM (Session Scope)                             │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  • Conversation buffer (ring buffer, last N messages)   │    │
│  │  • Tool call/result pairs                               │    │
│  │  • Current task scratchpad                              │    │
│  │  • Working variables                                    │    │
│  │                                                         │    │
│  │  Storage: In-memory + session_state.json               │    │
│  │  Lifetime: Single session                              │    │
│  │  Access: O(1) direct                                   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  TIER 2: WORKING MEMORY (Context Window)                        │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  • Compressed summaries of older turns                  │    │
│  │  • Active tool definitions (loaded on demand)           │    │
│  │  • Current reasoning chain                              │    │
│  │  • Retrieved episodic memories                          │    │
│  │                                                         │    │
│  │  Storage: Dynamically constructed                      │    │
│  │  Lifetime: Current inference call                      │    │
│  │  Access: Context builder assembles                     │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  TIER 3: LONG-TERM (Persistent)                                 │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │    │
│  │  │    User      │  │   Project    │  │   Episodic   │  │    │
│  │  │  Preferences │  │  Knowledge   │  │   Memory     │  │    │
│  │  │              │  │              │  │              │  │    │
│  │  │ ~/.kenetic/  │  │ ./KENETIC.md │  │ episodes/    │  │    │
│  │  │ user.md      │  │              │  │ *.json       │  │    │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  │    │
│  │                                                         │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │    │
│  │  │    Entity    │  │    Cross     │  │  Checkpoint  │  │    │
│  │  │    Memory    │  │    Thread    │  │    Store     │  │    │
│  │  │              │  │              │  │              │  │    │
│  │  │ entities.json│  │ facts.json   │  │ checkpoints/ │  │    │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  │    │
│  │                                                         │    │
│  │  Storage: File-based (JSON/Markdown)                   │    │
│  │  Lifetime: Persistent across sessions                  │    │
│  │  Access: Load on demand, index for search              │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 1.2 Memory File Formats

**User Memory** (`~/.kenetic/user.md`):
```markdown
# User Preferences

## Coding Style
- Prefer modern C++20 features
- Use snake_case for variables
- Always add const where possible

## Communication
- Be concise
- Avoid emojis
- Provide code examples

## Project Defaults
- Build system: CMake
- Testing: GoogleTest
- Formatting: clang-format
```

**Project Memory** (`./KENETIC.md`):
```markdown
# Project: KeneticCore

## Architecture Decisions
- File-based memory over vector DB for accuracy
- TRM for tool selection optimization
- MCP-compatible tool protocol

## Code Conventions
- Namespace: kenetic::
- Header guards: KENETIC_MODULE_FILE_HPP
- Error handling: Expected<T, Error>

## Key Files
- src/agent/orchestrator.cpp: Main agent loop
- src/trm/trm_model.cpp: TRM implementation
```

**Session State** (`storage/sessions/{id}/state.json`):
```json
{
  "session_id": "sess_abc123",
  "created_at": 1733400000,
  "updated_at": 1733401234,
  "conversation_turn": 15,
  "current_task": {
    "description": "Implement memory manager",
    "status": "in_progress",
    "started_at": 1733400500
  },
  "scratchpad": {
    "files_modified": ["src/memory/manager.cpp"],
    "pending_actions": []
  },
  "tool_state": {
    "last_tool": "file_write",
    "last_result": "success"
  }
}
```

**Thread Memory** (`storage/sessions/{id}/thread.jsonl`):
```jsonl
{"turn":1,"role":"user","content":"Help me implement memory","ts":1733400000}
{"turn":2,"role":"assistant","content":"I'll help...","ts":1733400005,"tool_calls":[...]}
{"turn":3,"role":"tool","name":"file_read","result":"...","ts":1733400010}
```

**Checkpoint** (`storage/checkpoints/{id}.json`):
```json
{
  "checkpoint_id": "cp_xyz789",
  "session_id": "sess_abc123",
  "thread_id": "thread_main",
  "timestamp": 1733401234,
  "parent_checkpoint": "cp_xyz788",
  "state": {
    "conversation_turn": 15,
    "memory_snapshot": {...},
    "tool_state": {...}
  },
  "metadata": {
    "trigger": "manual",
    "description": "Before refactoring"
  }
}
```

**Episodic Memory** (`storage/episodic/{id}.json`):
```json
{
  "episode_id": "ep_def456",
  "timestamp": 1733400000,
  "task": {
    "description": "Fix memory leak in session manager",
    "category": "bug_fix"
  },
  "context": {
    "project": "keneticCore",
    "files_involved": ["src/memory/session.cpp"]
  },
  "actions": [
    {"tool": "file_read", "args": {"path": "..."}, "result": "success"},
    {"tool": "file_edit", "args": {...}, "result": "success"}
  ],
  "outcome": {
    "success": true,
    "turns_taken": 5,
    "tools_used": 3
  },
  "learnings": [
    "Session cleanup requires explicit destructor call",
    "Use RAII pattern for resource management"
  ],
  "embedding": null
}
```

**Episodic Index** (`storage/episodic/index.json`):
```json
{
  "ep_def456": {
    "task_keywords": ["memory", "leak", "fix", "session"],
    "category": "bug_fix",
    "success": true,
    "timestamp": 1733400000,
    "turns": 5
  }
}
```

---

### 2. TRM Reasoning Engine

#### 2.1 TRM Architecture (from paper)

```
┌─────────────────────────────────────────────────────────────────┐
│                    TRM RECURSIVE REASONING                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Input: x (embedded task + context)                             │
│  States: y (current answer), z (reasoning latent)               │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │    ┌─────┐     ┌─────┐     ┌─────┐                     │    │
│  │    │  x  │     │  y  │     │  z  │                     │    │
│  │    └──┬──┘     └──┬──┘     └──┬──┘                     │    │
│  │       │           │           │                         │    │
│  │       └───────────┼───────────┘                         │    │
│  │                   │                                     │    │
│  │                   ▼                                     │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │              2-Layer Transformer                 │   │    │
│  │  │  ┌─────────────────────────────────────────┐    │   │    │
│  │  │  │  RMSNorm → Self-Attn → RMSNorm → MLP   │×2  │   │    │
│  │  │  │  (No bias, RoPE, SwiGLU)                │    │   │    │
│  │  │  └─────────────────────────────────────────┘    │   │    │
│  │  │                    ~7M parameters               │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                   │                                     │    │
│  │       ┌───────────┴───────────┐                         │    │
│  │       │                       │                         │    │
│  │       ▼                       ▼                         │    │
│  │    ┌─────┐                 ┌─────┐                      │    │
│  │    │ z'  │ (n=6 times)     │ y'  │ (once per cycle)    │    │
│  │    └─────┘                 └─────┘                      │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Recursion Pattern (T=3, n=6):                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │  for j in 0..T-1:           # Deep recursion (no grad)  │    │
│  │      for i in 0..n:         # Latent recursion          │    │
│  │          z = net(x, y, z)   # Update reasoning          │    │
│  │      y = net(y, z)          # Update answer             │    │
│  │                                                         │    │
│  │  # Final pass with gradients                            │    │
│  │  for i in 0..n:                                         │    │
│  │      z = net(x, y, z)                                   │    │
│  │  y = net(y, z)                                          │    │
│  │                                                         │    │
│  │  Effective depth: T × (n+1) × 2 = 3 × 7 × 2 = 42 layers│    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Deep Supervision (N_sup = 16 max improvement steps):           │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │  y, z = init()                                          │    │
│  │  for step in 0..N_sup:                                  │    │
│  │      y, z = deep_recursion(x, y, z)                     │    │
│  │      loss = cross_entropy(output_head(y), target)       │    │
│  │      if q_head(y) > 0:  # Learned halting               │    │
│  │          break                                          │    │
│  │      y, z = detach(y, z)  # Stop gradients              │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 2.2 TRM Applications in KeneticCore

```
┌─────────────────────────────────────────────────────────────────┐
│                    TRM USE CASES                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. TOOL SELECTION OPTIMIZER                                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Input x: [task_embedding, context_embedding, tool_list]│    │
│  │  Output y: [tool_id, confidence, parameters_hint]       │    │
│  │  Training: Episodes where tool choice led to success    │    │
│  │                                                         │    │
│  │  Example:                                               │    │
│  │    Task: "Find all TODO comments"                       │    │
│  │    TRM Output: tool=grep, pattern="TODO", confidence=0.9│    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  2. ACTION SEQUENCE PLANNER                                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Input x: [task_embedding, current_state]               │    │
│  │  Output y: [action_sequence_embedding]                  │    │
│  │  Decoded: ["read file", "analyze", "edit", "test"]      │    │
│  │                                                         │    │
│  │  Example:                                               │    │
│  │    Task: "Fix the bug in auth module"                   │    │
│  │    TRM Output: [read→analyze→hypothesis→edit→test]      │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  3. CONTEXT RELEVANCE SCORER                                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Input x: [query_embedding, memory_chunk_embeddings]    │    │
│  │  Output y: [relevance_scores for each chunk]            │    │
│  │  Used for: Selecting which memories to include          │    │
│  │                                                         │    │
│  │  Example:                                               │    │
│  │    Query: "How did we fix memory leaks before?"         │    │
│  │    TRM scores episodes, returns top-K relevant          │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  4. RESPONSE QUALITY PREDICTOR                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Input x: [task, proposed_response_embedding]           │    │
│  │  Output y: [quality_score, suggested_improvements]      │    │
│  │  Used for: Self-critique before responding              │    │
│  │                                                         │    │
│  │  Example:                                               │    │
│  │    Task: "Explain recursion"                            │    │
│  │    Response draft → TRM scores 0.7, suggests example    │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 2.3 TRM Training Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                    TRM TRAINING PIPELINE                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────┐                                           │
│  │  Episode Buffer  │  Successful agent interactions            │
│  │  (File-based)    │  stored in episodic memory                │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │  Data Augmentor  │  • Dihedral transforms (rotations, flips) │
│  │                  │  • Task paraphrasing                      │
│  │                  │  • Action sequence shuffling              │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │  Training Loop   │                                           │
│  │                  │                                           │
│  │  for epoch:                                                  │
│  │    for batch in dataloader:                                  │
│  │      y, z = init()                                           │
│  │      for step in N_sup:                                      │
│  │        y, z = deep_recursion(x, y, z)                        │
│  │        loss = CE(y, target) + BCE(q, correct)                │
│  │        loss.backward()                                       │
│  │        if q > 0: break  # ACT early stopping                 │
│  │        y, z = detach(y, z)                                   │
│  │      optimizer.step()                                        │
│  │      ema_update(model)                                       │
│  │                                                              │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │  Model Export    │  Save to data/trm_weights/                │
│  │  (ONNX/TorchScript)                                          │
│  └──────────────────┘                                           │
│                                                                  │
│  Hyperparameters (from TRM paper):                              │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  • Optimizer: AdamW (β1=0.9, β2=0.95)                   │    │
│  │  • Learning rate: 1e-4 (1e-2 for embeddings)            │    │
│  │  • Weight decay: 0.1 - 1.0                              │    │
│  │  • Batch size: 768                                      │    │
│  │  • Hidden size: 512                                     │    │
│  │  • Layers: 2                                            │    │
│  │  • Recursion: T=3, n=6                                  │    │
│  │  • N_sup: 16                                            │    │
│  │  • EMA: 0.999                                           │    │
│  │  • Loss: Stable-max (for stability)                     │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 2.4 TRM Unsupervised Learning Strategy

KeneticCore implements **unsupervised learning** for TRM, where the model learns patterns from raw interaction data without requiring labeled LLM examples. TRM learns to predict optimal tool sequences through self-supervised objectives.

```
┌─────────────────────────────────────────────────────────────────┐
│             TRM UNSUPERVISED LEARNING ARCHITECTURE               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CORE PRINCIPLE: Learn from outcomes, not from LLM labels       │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │               SELF-SUPERVISED OBJECTIVES                 │    │
│  │                                                         │    │
│  │  1. CONTRASTIVE LEARNING (Tool-Context Alignment)       │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  • Positive pairs: (context, successful_tool)     │  │    │
│  │  │  • Negative pairs: (context, failed_tool)         │  │    │
│  │  │  • Objective: Maximize similarity of positives,   │  │    │
│  │  │              minimize similarity of negatives     │  │    │
│  │  │                                                   │  │    │
│  │  │  Loss = -log(exp(sim(c,t+)/τ) /                   │  │    │
│  │  │             Σ exp(sim(c,ti)/τ))                   │  │    │
│  │  │                                                   │  │    │
│  │  │  No LLM labels needed - success/fail is ground truth│  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  2. NEXT-ACTION PREDICTION (Autoregressive)             │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  • Input: Sequence of (context, tool) pairs       │  │    │
│  │  │  • Predict: Next tool in sequence                 │  │    │
│  │  │  • Self-supervised: Learn patterns from traces    │  │    │
│  │  │                                                   │  │    │
│  │  │  P(tool_t | context, tool_1, ..., tool_{t-1})     │  │    │
│  │  │                                                   │  │    │
│  │  │  TRM recursion discovers latent action patterns   │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  3. OUTCOME PREDICTION (Reward Modeling)                │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  • Input: (context, tool_sequence)                │  │    │
│  │  │  • Predict: Binary success/failure                │  │    │
│  │  │  • Self-supervised: Outcome is ground truth       │  │    │
│  │  │                                                   │  │    │
│  │  │  TRM learns: Which tool sequences lead to success?│  │    │
│  │  │  No human labels - task completion is the signal  │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  4. MASKED TOOL PREDICTION (BERT-style)                 │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  • Input: Sequence with [MASK] tokens             │  │    │
│  │  │  • Predict: Masked tools from context             │  │    │
│  │  │  • Self-supervised: No external labels needed     │  │    │
│  │  │                                                   │  │    │
│  │  │  [read_file, MASK, edit_file, MASK, run_test]     │  │    │
│  │  │       → predict: [analyze, verify]                │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │               DATA COLLECTION (No Labels)                │    │
│  │                                                         │    │
│  │  Every interaction automatically generates training data:│    │
│  │                                                         │    │
│  │  Episode = {                                            │    │
│  │    context: embedded(user_task + project_context),      │    │
│  │    trajectory: [                                        │    │
│  │      {tool: "file_read", params: {...}, result: "ok"},  │    │
│  │      {tool: "grep", params: {...}, result: "ok"},       │    │
│  │      {tool: "file_edit", params: {...}, result: "ok"},  │    │
│  │    ],                                                   │    │
│  │    outcome: "success" | "failure",  // Ground truth     │    │
│  │    duration: 45s,                                       │    │
│  │    tool_count: 3                                        │    │
│  │  }                                                      │    │
│  │                                                         │    │
│  │  Labels derived automatically:                          │    │
│  │  • Success: Task completed without user retry           │    │
│  │  • Failure: User had to rephrase, error occurred        │    │
│  │  • Efficiency: Fewer tools + faster = better            │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │               TRAINING PHASES                            │    │
│  │                                                         │    │
│  │  PHASE 1: COLD START (Pre-training)                     │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  • Collect N interactions (configurable, default 100)│  │    │
│  │  │  • No TRM inference during this phase              │  │    │
│  │  │  • Just record: (context, tools, outcome)          │  │    │
│  │  │  • LLM handles all decisions                       │  │    │
│  │  │                                                    │  │    │
│  │  │  Trigger training when: episodes >= min_episodes   │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                              │                           │    │
│  │                              ▼                           │    │
│  │  PHASE 2: UNSUPERVISED TRAINING (Background)            │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  Combined loss (multi-task unsupervised):         │  │    │
│  │  │                                                   │  │    │
│  │  │  L = λ1 * L_contrastive    // Tool alignment      │  │    │
│  │  │    + λ2 * L_next_action    // Sequence modeling   │  │    │
│  │  │    + λ3 * L_outcome        // Success prediction  │  │    │
│  │  │    + λ4 * L_masked         // Fill-in-the-blank   │  │    │
│  │  │                                                   │  │    │
│  │  │  Default weights: λ1=1.0, λ2=0.5, λ3=0.3, λ4=0.2 │  │    │
│  │  │                                                   │  │    │
│  │  │  Training runs in background thread               │  │    │
│  │  │  Agent continues operating during training        │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                              │                           │    │
│  │                              ▼                           │    │
│  │  PHASE 3: ONLINE LEARNING (Continuous)                  │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  • TRM provides tool recommendations              │  │    │
│  │  │  • New episodes feed back into training           │  │    │
│  │  │  • Periodic retraining (configurable interval)    │  │    │
│  │  │  • EMA model updates for stability (0.999)        │  │    │
│  │  │                                                   │  │    │
│  │  │  Self-improving loop:                             │  │    │
│  │  │  interact → collect → train → improve → repeat    │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │               GRACEFUL DEGRADATION                       │    │
│  │                                                         │    │
│  │  If TRM fails or not yet trained:                       │    │
│  │  • Fall back to rule-based tool selection               │    │
│  │  • Keyword matching: task → tool keywords               │    │
│  │  • Tool category heuristics (file task → file tools)    │    │
│  │  • Continue collecting data for training                │    │
│  │                                                         │    │
│  │  No LLM fallback needed - rules handle cold start       │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Key Benefits of Unsupervised Learning:**

| Approach | LLM Labels | Training Data | Cost |
|----------|------------|---------------|------|
| Supervised | Required | LLM examples | High (API costs) |
| **Unsupervised** | **None** | **Interaction logs** | **Zero marginal** |

- **No LLM labeling costs**: Learning signal comes from outcomes, not LLM judgments
- **Continuous improvement**: Every interaction is training data
- **Domain adaptation**: Naturally adapts to user's specific usage patterns
- **Privacy-preserving**: No data sent to LLM for labeling

**Configuration:**

```yaml
trm:
  enabled: true
  mode: unsupervised                    # unsupervised | supervised
  min_episodes_before_training: 100     # Configurable N for cold start

  # Unsupervised learning weights
  loss_weights:
    contrastive: 1.0       # Tool-context alignment
    next_action: 0.5       # Sequence prediction
    outcome: 0.3           # Success prediction
    masked: 0.2            # Masked tool prediction

  # Training schedule
  retrain_interval_hours: 24
  ema_decay: 0.999

  # Fallback behavior
  fallback_mode: rules     # rules | keyword | disabled
```

---

### 3. Tool System

#### 3.1 Tool Registry Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       TOOL SYSTEM                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    TOOL REGISTRY                         │    │
│  │                                                         │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  Tool Discovery                                    │  │    │
│  │  │  • Static registration (compile-time)             │  │    │
│  │  │  • Dynamic loading (runtime plugins)              │  │    │
│  │  │  • MCP server connection                          │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                         │                               │    │
│  │                         ▼                               │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  Tool Index                                        │  │    │
│  │  │  ┌─────────────────────────────────────────────┐  │  │    │
│  │  │  │  tool_id: "file_read"                       │  │  │    │
│  │  │  │  spec: ToolSpec {...}                       │  │  │    │
│  │  │  │  handler: function pointer                  │  │  │    │
│  │  │  │  keywords: ["read", "file", "content"]      │  │  │    │
│  │  │  │  cost_estimate: 0.1                         │  │  │    │
│  │  │  │  requires_confirmation: false               │  │  │    │
│  │  │  └─────────────────────────────────────────────┘  │  │    │
│  │  │  ┌─────────────────────────────────────────────┐  │  │    │
│  │  │  │  tool_id: "bash"                            │  │  │    │
│  │  │  │  ...                                        │  │  │    │
│  │  │  └─────────────────────────────────────────────┘  │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                         │                               │    │
│  │                         ▼                               │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  Tool Selector (TRM-assisted)                      │  │    │
│  │  │                                                    │  │    │
│  │  │  Input: task_context + available_tools            │  │    │
│  │  │  Process:                                         │  │    │
│  │  │    1. Keyword filter (fast, reduce candidates)    │  │    │
│  │  │    2. TRM scoring (recursive refinement)          │  │    │
│  │  │    3. Return ranked tool list                     │  │    │
│  │  │  Output: [(tool_id, score, param_hints), ...]     │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    TOOL EXECUTOR                         │    │
│  │                                                         │    │
│  │  execute(tool_id, params):                              │    │
│  │    1. Validate params against schema                    │    │
│  │    2. Check permissions / confirmation                  │    │
│  │    3. Execute handler                                   │    │
│  │    4. Capture result + timing                           │    │
│  │    5. Log to episode for training                       │    │
│  │    6. Return ToolResult                                 │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 3.2 Tool Specification Schema

```
┌─────────────────────────────────────────────────────────────────┐
│                    TOOL SPEC SCHEMA                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  {                                                               │
│    "name": "file_read",                                         │
│    "description": "Read contents of a file from disk",          │
│    "version": "1.0.0",                                          │
│                                                                  │
│    "input_schema": {                                            │
│      "type": "object",                                          │
│      "properties": {                                            │
│        "path": {                                                │
│          "type": "string",                                      │
│          "description": "Absolute path to file"                 │
│        },                                                        │
│        "offset": {                                              │
│          "type": "integer",                                     │
│          "description": "Line to start reading from",           │
│          "default": 0                                           │
│        },                                                        │
│        "limit": {                                               │
│          "type": "integer",                                     │
│          "description": "Max lines to read",                    │
│          "default": 2000                                        │
│        }                                                         │
│      },                                                          │
│      "required": ["path"]                                       │
│    },                                                            │
│                                                                  │
│    "output_schema": {                                           │
│      "type": "object",                                          │
│      "properties": {                                            │
│        "content": {"type": "string"},                           │
│        "total_lines": {"type": "integer"},                      │
│        "truncated": {"type": "boolean"}                         │
│      }                                                           │
│    },                                                            │
│                                                                  │
│    "metadata": {                                                │
│      "category": "file_system",                                 │
│      "keywords": ["read", "file", "content", "view"],           │
│      "cost_estimate": 0.1,                                      │
│      "typical_latency_ms": 50,                                  │
│      "requires_confirmation": false,                            │
│      "side_effects": false                                      │
│    },                                                            │
│                                                                  │
│    "examples": [                                                │
│      {                                                          │
│        "description": "Read a Python file",                     │
│        "input": {"path": "/project/main.py"},                   │
│        "output": {"content": "...", "total_lines": 100}         │
│      }                                                           │
│    ]                                                             │
│  }                                                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 3.3 MCP Protocol Integration

```
┌─────────────────────────────────────────────────────────────────┐
│                    MCP INTEGRATION                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  KeneticCore acts as MCP HOST + CLIENT                          │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                     MCP HOST                              │   │
│  │                                                           │   │
│  │  KeneticCore                                              │   │
│  │       │                                                   │   │
│  │       ├──► MCP Client 1 ──► External MCP Server (DB)     │   │
│  │       │                                                   │   │
│  │       ├──► MCP Client 2 ──► External MCP Server (Git)    │   │
│  │       │                                                   │   │
│  │       └──► MCP Client 3 ──► External MCP Server (API)    │   │
│  │                                                           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    MCP SERVER                             │   │
│  │                                                           │   │
│  │  KeneticCore exposes its tools via MCP for other agents  │   │
│  │                                                           │   │
│  │  External Agent ──► MCP ──► KeneticCore Tools            │   │
│  │                                                           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  Protocol: JSON-RPC 2.0 over stdio/HTTP/WebSocket               │
│                                                                  │
│  Message Types:                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Request:  {"jsonrpc":"2.0","id":1,"method":"..."}      │    │
│  │  Response: {"jsonrpc":"2.0","id":1,"result":{...}}      │    │
│  │  Error:    {"jsonrpc":"2.0","id":1,"error":{...}}       │    │
│  │  Notify:   {"jsonrpc":"2.0","method":"...","params":{}} │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Key Methods:                                                    │
│  • tools/list         - Get available tools                     │
│  • tools/call         - Execute a tool                          │
│  • resources/list     - List available resources                │
│  • resources/read     - Read a resource                         │
│  • prompts/list       - List prompt templates                   │
│  • prompts/get        - Get a prompt template                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 4. Context Management

#### 4.1 Context Window Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    CONTEXT MANAGEMENT                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Context Budget: ~200K tokens (Claude) / ~1M tokens (Gemini)    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 CONTEXT ALLOCATION                       │    │
│  │                                                         │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  System Prompt                        ~2K tokens │   │    │
│  │  │  • Agent identity and capabilities               │   │    │
│  │  │  • Core instructions                             │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                         │                               │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  User Memory (KENETIC.md)            ~1K tokens │   │    │
│  │  │  • User preferences                              │   │    │
│  │  │  • Project context                               │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                         │                               │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  Tool Definitions                   ~5-20K tokens│   │    │
│  │  │  • Active tools only (not all registered)        │   │    │
│  │  │  • Selected by TRM based on task                 │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                         │                               │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  Episodic Memory (retrieved)         ~2K tokens │   │    │
│  │  │  • Relevant past experiences                     │   │    │
│  │  │  • Similar task solutions                        │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                         │                               │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  Compressed History                 ~5-10K tokens│   │    │
│  │  │  • Summarized older turns                        │   │    │
│  │  │  • Key decisions and outcomes                    │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                         │                               │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  Recent History (raw)             ~50-150K tokens│   │    │
│  │  │  • Last 10 turns verbatim                        │   │    │
│  │  │  • Full tool calls and results                   │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                         │                               │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  Current Turn                       Variable     │   │    │
│  │  │  • User message                                  │   │    │
│  │  │  • Any attached files/images                     │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                         │                               │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  Reserved for Response              ~30K tokens │   │    │
│  │  │  • Agent's response + tool calls                 │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 4.2 Context Compaction Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                    CONTEXT COMPACTION                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Trigger: context_tokens > threshold (e.g., 150K)               │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  COMPACTION STRATEGIES                   │    │
│  │                                                         │    │
│  │  Strategy 1: OBSERVATION MASKING (fast, lossless keys)  │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  Before:                                          │  │    │
│  │  │    Tool: file_read                                │  │    │
│  │  │    Result: [2000 lines of code...]               │  │    │
│  │  │                                                   │  │    │
│  │  │  After:                                           │  │    │
│  │  │    Tool: file_read                                │  │    │
│  │  │    Result: [content omitted - 2000 lines]        │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  Strategy 2: LLM SUMMARIZATION (slower, higher quality) │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  Before: 21 conversation turns (detailed)         │  │    │
│  │  │                                                   │  │    │
│  │  │  After: Bullet-point summary                      │  │    │
│  │  │    • User requested feature X                     │  │    │
│  │  │    • Implemented in src/foo.cpp                   │  │    │
│  │  │    • Tests passing                                │  │    │
│  │  │    • Blocker: missing dependency Y                │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  Strategy 3: HIERARCHICAL (multi-level compression)     │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  Level 0: Raw turns (last 10)                     │  │    │
│  │  │  Level 1: Summaries (turns 11-50)                 │  │    │
│  │  │  Level 2: Key facts only (turns 51+)              │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 COMPACTION PIPELINE                      │    │
│  │                                                         │    │
│  │  1. Count tokens in current context                     │    │
│  │  2. If under threshold, skip compaction                 │    │
│  │  3. Identify compactable segments:                      │    │
│  │     - Old tool results (mask observations)              │    │
│  │     - Old conversation turns (summarize)                │    │
│  │  4. Apply strategies in order of token savings          │    │
│  │  5. Persist compaction state for incremental updates    │    │
│  │  6. Return compacted context                            │    │
│  │                                                         │    │
│  │  Optimal parameters (from research):                    │    │
│  │  • Keep raw: 10 turns                                   │    │
│  │  • Summarize batch: 21 turns                            │    │
│  │  • Memory reduction: 26-54%                             │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 4.3 Context Editor (Stale Content Removal)

```
┌─────────────────────────────────────────────────────────────────┐
│                    CONTEXT EDITOR                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Purpose: Remove stale tool calls/results that are no longer    │
│           relevant to current task                              │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 STALENESS DETECTION                      │    │
│  │                                                         │    │
│  │  Stale if:                                              │    │
│  │  • File was read but later modified (content outdated)  │    │
│  │  • Search results superseded by newer search            │    │
│  │  • Failed tool call with successful retry               │    │
│  │  • Exploratory reads not used in final solution         │    │
│  │                                                         │    │
│  │  Keep if:                                               │    │
│  │  • Recent (within last N turns)                         │    │
│  │  • Referenced in current task                           │    │
│  │  • Contains error info still relevant                   │    │
│  │  • Part of successful action sequence                   │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 EDITING OPERATIONS                       │    │
│  │                                                         │    │
│  │  1. TRUNCATE: Replace large result with summary         │    │
│  │     [2000 lines] → [first 50 lines... truncated]       │    │
│  │                                                         │    │
│  │  2. REMOVE: Delete entire tool call/result pair         │    │
│  │     Keeps: "Previously read file X"                     │    │
│  │                                                         │    │
│  │  3. MERGE: Combine related tool calls                   │    │
│  │     3 grep searches → "Searched for X, Y, Z in /src"   │    │
│  │                                                         │    │
│  │  4. REFERENCE: Replace content with reference           │    │
│  │     Full file → "See src/main.cpp (read earlier)"      │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 5. LLM Gateway

#### 5.1 Multi-Provider Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      LLM GATEWAY                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  UNIFIED INTERFACE                       │    │
│  │                                                         │    │
│  │  class LLMGateway {                                     │    │
│  │      Result<Response> complete(Request);                │    │
│  │      AsyncResult<Response> stream(Request);             │    │
│  │      int count_tokens(string);                          │    │
│  │      vector<float> embed(string);                       │    │
│  │  };                                                     │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│          ┌───────────────────┼───────────────────┐              │
│          │                   │                   │              │
│          ▼                   ▼                   ▼              │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐       │
│  │    Claude     │  │    Gemini     │  │    Local      │       │
│  │   Provider    │  │   Provider    │  │   Provider    │       │
│  │               │  │               │  │               │       │
│  │ • Opus 4.5    │  │ • 3 Pro       │  │ • llama.cpp   │       │
│  │ • Sonnet 4    │  │ • 2.5 Flash   │  │ • Qwen        │       │
│  │ • Haiku 3.5   │  │               │  │ • Mistral     │       │
│  │               │  │               │  │               │       │
│  │ Context: 200K │  │ Context: 1M+  │  │ Context: var  │       │
│  └───────────────┘  └───────────────┘  └───────────────┘       │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  PROVIDER SELECTION                      │    │
│  │                                                         │    │
│  │  Routing Strategy:                                      │    │
│  │  • Primary: User-specified (Claude Opus / Gemini Pro)   │    │
│  │  • Fallback: On rate limit or error                     │    │
│  │  • Cost optimization: Haiku for simple tasks            │    │
│  │  • Context overflow: Gemini for very long contexts      │    │
│  │                                                         │    │
│  │  Model Selection Heuristics:                            │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  Task Type          │ Recommended Model           │  │    │
│  │  ├─────────────────────┼─────────────────────────────┤  │    │
│  │  │  Complex reasoning  │ Claude Opus 4.5             │  │    │
│  │  │  Code generation    │ Claude Opus 4.5 / Gemini    │  │    │
│  │  │  Long context       │ Gemini 3 Pro (1M context)   │  │    │
│  │  │  Quick tasks        │ Claude Haiku / Gemini Flash │  │    │
│  │  │  Summarization      │ Claude Haiku                │  │    │
│  │  │  Embeddings         │ Local model / API           │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  REQUEST/RESPONSE                        │    │
│  │                                                         │    │
│  │  Request {                                              │    │
│  │    messages: vector<Message>                            │    │
│  │    tools: vector<ToolSpec>                              │    │
│  │    model: optional<string>                              │    │
│  │    max_tokens: int                                      │    │
│  │    temperature: float                                   │    │
│  │    stop_sequences: vector<string>                       │    │
│  │    stream: bool                                         │    │
│  │  }                                                      │    │
│  │                                                         │    │
│  │  Response {                                             │    │
│  │    content: string                                      │    │
│  │    tool_calls: vector<ToolCall>                         │    │
│  │    stop_reason: StopReason                              │    │
│  │    usage: {input_tokens, output_tokens}                 │    │
│  │    model: string                                        │    │
│  │    latency_ms: int                                      │    │
│  │  }                                                      │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 6. Agent Orchestrator

#### 6.1 Main Agent Loop

```
┌─────────────────────────────────────────────────────────────────┐
│                    AGENT ORCHESTRATOR                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                   MAIN LOOP                              │    │
│  │                                                         │    │
│  │  while (!session.ended) {                               │    │
│  │                                                         │    │
│  │    // 1. Receive user input                             │    │
│  │    Message user_msg = await_input();                    │    │
│  │    memory.append_to_thread(user_msg);                   │    │
│  │                                                         │    │
│  │    // 2. Build context                                  │    │
│  │    Context ctx = context_builder.build(                 │    │
│  │      system_prompt,                                     │    │
│  │      memory.get_user_prefs(),                           │    │
│  │      memory.get_project_context(),                      │    │
│  │      memory.retrieve_episodes(user_msg),                │    │
│  │      memory.get_compressed_history(),                   │    │
│  │      memory.get_recent_turns(10),                       │    │
│  │      user_msg                                           │    │
│  │    );                                                   │    │
│  │                                                         │    │
│  │    // 3. Select relevant tools (TRM-assisted)           │    │
│  │    auto tools = tool_registry.select(                   │    │
│  │      ctx, trm_tool_selector                             │    │
│  │    );                                                   │    │
│  │                                                         │    │
│  │    // 4. Plan (optional, for complex tasks)             │    │
│  │    if (planner.should_plan(ctx)) {                      │    │
│  │      Plan plan = planner.create_plan(ctx);              │    │
│  │      ctx.add_plan(plan);                                │    │
│  │    }                                                    │    │
│  │                                                         │    │
│  │    // 5. Execute agent turn                             │    │
│  │    do {                                                 │    │
│  │      Response resp = llm.complete(ctx, tools);          │    │
│  │                                                         │    │
│  │      if (resp.has_tool_calls()) {                       │    │
│  │        for (auto& tc : resp.tool_calls) {               │    │
│  │          ToolResult result = executor.execute(tc);      │    │
│  │          ctx.add_tool_result(result);                   │    │
│  │          memory.log_tool_call(tc, result);              │    │
│  │        }                                                │    │
│  │      } else {                                           │    │
│  │        // Final response                                │    │
│  │        display(resp.content);                           │    │
│  │        memory.append_to_thread(resp);                   │    │
│  │        break;                                           │    │
│  │      }                                                  │    │
│  │                                                         │    │
│  │      // Check context limits                            │    │
│  │      if (ctx.needs_compaction()) {                      │    │
│  │        ctx = compactor.compact(ctx);                    │    │
│  │      }                                                  │    │
│  │                                                         │    │
│  │    } while (resp.stop_reason == TOOL_USE);              │    │
│  │                                                         │    │
│  │    // 6. Evaluate and learn                             │    │
│  │    evaluator.record_episode(ctx, outcome);              │    │
│  │                                                         │    │
│  │    // 7. Checkpoint if significant                      │    │
│  │    if (should_checkpoint(ctx)) {                        │    │
│  │      memory.create_checkpoint();                        │    │
│  │    }                                                    │    │
│  │  }                                                      │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 6.2 Multi-Agent Orchestration

```
┌─────────────────────────────────────────────────────────────────┐
│                  MULTI-AGENT PATTERNS                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Pattern 1: SEQUENTIAL                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │  Task ──► Agent A ──► Agent B ──► Agent C ──► Result   │    │
│  │           (Plan)      (Execute)    (Review)             │    │
│  │                                                         │    │
│  │  Use case: Code review pipeline                         │    │
│  │  • Planner agent creates implementation plan            │    │
│  │  • Coder agent implements                               │    │
│  │  • Reviewer agent validates                             │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Pattern 2: PARALLEL                                            │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │         ┌──► Agent A (Search DB) ──┐                   │    │
│  │         │                          │                   │    │
│  │  Task ──┼──► Agent B (Search Web) ─┼──► Aggregator     │    │
│  │         │                          │                   │    │
│  │         └──► Agent C (Search Code)─┘                   │    │
│  │                                                         │    │
│  │  Use case: Information gathering                        │    │
│  │  • Multiple search agents run concurrently              │    │
│  │  • Aggregator combines and deduplicates                 │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Pattern 3: HIERARCHICAL                                        │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │                    ┌─────────────┐                      │    │
│  │                    │  Manager    │                      │    │
│  │                    │   Agent     │                      │    │
│  │                    └──────┬──────┘                      │    │
│  │           ┌───────────────┼───────────────┐             │    │
│  │           ▼               ▼               ▼             │    │
│  │    ┌───────────┐   ┌───────────┐   ┌───────────┐       │    │
│  │    │  Worker   │   │  Worker   │   │  Worker   │       │    │
│  │    │  Agent 1  │   │  Agent 2  │   │  Agent 3  │       │    │
│  │    └───────────┘   └───────────┘   └───────────┘       │    │
│  │                                                         │    │
│  │  Use case: Complex project implementation               │    │
│  │  • Manager decomposes task, assigns to workers          │    │
│  │  • Workers report progress, ask for clarification       │    │
│  │  • Manager integrates results                           │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Pattern 4: LOOP (Iterative Refinement)                         │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │           ┌────────────────────────────┐                │    │
│  │           │                            │                │    │
│  │           ▼                            │                │    │
│  │  Task ──► Generator ──► Critic ──► Pass? ──► Result    │    │
│  │              ▲            │           │                 │    │
│  │              │            │      No   │                 │    │
│  │              └────────────┴───────────┘                 │    │
│  │                                                         │    │
│  │  Use case: High-quality content generation              │    │
│  │  • Generator creates draft                              │    │
│  │  • Critic evaluates and provides feedback               │    │
│  │  • Loop until quality threshold met                     │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Sub-Agent Communication:                                       │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  • Each sub-agent gets clean context window             │    │
│  │  • Returns condensed summary (1-2K tokens)              │    │
│  │  • Parent agent integrates summaries                    │    │
│  │  • Isolation prevents context pollution                 │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 7. Reward & Training System

#### 7.1 Reward Signal Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    REWARD SYSTEM                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 REWARD COMPONENTS                        │    │
│  │                                                         │    │
│  │  R_total = w1*R_task + w2*R_efficiency + w3*R_quality   │    │
│  │                                                         │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  R_task (Task Completion)                         │  │    │
│  │  │  • Binary: Did task succeed? (0 or 1)             │  │    │
│  │  │  • Partial: % of objectives completed             │  │    │
│  │  │  • Verified: Automated test results               │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  R_efficiency (Resource Usage)                    │  │    │
│  │  │  • Token efficiency: fewer tokens = higher reward │  │    │
│  │  │  • Tool efficiency: fewer calls = higher reward   │  │    │
│  │  │  • Time efficiency: faster = higher reward        │  │    │
│  │  │  • Normalized against task complexity             │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  R_quality (Output Quality)                       │  │    │
│  │  │  • Code: passes linter, tests, type checks        │  │    │
│  │  │  • Text: coherence, relevance (LLM-as-judge)      │  │    │
│  │  │  • User feedback: explicit ratings                │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 STEPWISE REWARDS                         │    │
│  │                                                         │    │
│  │  Instead of final reward only, decompose into steps:    │    │
│  │                                                         │    │
│  │  Step 1: Read file     → r1 = 0.1 (necessary action)   │    │
│  │  Step 2: Analyze       → r2 = 0.2 (found issue)        │    │
│  │  Step 3: Edit file     → r3 = 0.3 (correct fix)        │    │
│  │  Step 4: Run tests     → r4 = 0.4 (tests pass)         │    │
│  │  ─────────────────────────────────────────────         │    │
│  │  Total: R = 1.0                                         │    │
│  │                                                         │    │
│  │  Benefits:                                              │    │
│  │  • Faster learning (intermediate feedback)              │    │
│  │  • Better credit assignment                             │    │
│  │  • Reward shaping for exploration                       │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 7.2 Training Data Collection

```
┌─────────────────────────────────────────────────────────────────┐
│                    DATA COLLECTION                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 EPISODE RECORDING                        │    │
│  │                                                         │    │
│  │  Every agent interaction records:                       │    │
│  │                                                         │    │
│  │  episode = {                                            │    │
│  │    task: "Fix bug in auth module",                      │    │
│  │    context: {project, files, user_prefs},               │    │
│  │    trajectory: [                                        │    │
│  │      {state, action, tool_call, result, reward},        │    │
│  │      {state, action, tool_call, result, reward},        │    │
│  │      ...                                                │    │
│  │    ],                                                   │    │
│  │    outcome: {success: true, turns: 5, tokens: 10000},   │    │
│  │    user_feedback: optional                              │    │
│  │  }                                                      │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 DATA AUGMENTATION                        │    │
│  │                                                         │    │
│  │  Augmentation techniques (from TRM paper):              │    │
│  │                                                         │    │
│  │  1. Task Paraphrasing                                   │    │
│  │     "Fix bug" → "Resolve issue" → "Debug problem"       │    │
│  │                                                         │    │
│  │  2. Action Reordering (when order doesn't matter)       │    │
│  │     [read A, read B, edit] → [read B, read A, edit]     │    │
│  │                                                         │    │
│  │  3. Context Variation                                   │    │
│  │     Same task with different surrounding context        │    │
│  │                                                         │    │
│  │  4. Noise Injection                                     │    │
│  │     Add irrelevant information to test robustness       │    │
│  │                                                         │    │
│  │  Target: 1000x augmentation per episode                 │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 8. Configuration System

```
┌─────────────────────────────────────────────────────────────────┐
│                    CONFIGURATION                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ~/.kenetic/config.yaml                                         │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  # LLM Configuration                                    │    │
│  │  llm:                                                   │    │
│  │    primary_provider: claude                             │    │
│  │    primary_model: claude-opus-4-5-20251101              │    │
│  │    fallback_provider: gemini                            │    │
│  │    fallback_model: gemini-3-pro                         │    │
│  │    summarization_model: claude-haiku-3-5                │    │
│  │                                                         │    │
│  │  # API Keys (or env vars)                               │    │
│  │  api_keys:                                              │    │
│  │    anthropic: ${ANTHROPIC_API_KEY}                      │    │
│  │    google: ${GOOGLE_API_KEY}                            │    │
│  │                                                         │    │
│  │  # Memory Configuration                                 │    │
│  │  memory:                                                │    │
│  │    storage_path: ~/.kenetic/storage                     │    │
│  │    max_episodes: 10000                                  │    │
│  │    checkpoint_interval: 10  # turns                     │    │
│  │                                                         │    │
│  │  # Context Configuration                                │    │
│  │  context:                                               │    │
│  │    max_tokens: 180000                                   │    │
│  │    compaction_threshold: 150000                         │    │
│  │    keep_raw_turns: 10                                   │    │
│  │    summarize_batch: 21                                  │    │
│  │                                                         │    │
│  │  # TRM Configuration (Unsupervised Learning)            │    │
│  │  trm:                                                   │    │
│  │    enabled: true                                        │    │
│  │    mode: unsupervised  # unsupervised | supervised      │    │
│  │    model_path: ~/.kenetic/models/trm_tool_selector.pt   │    │
│  │    min_episodes_before_training: 100  # Configurable N  │    │
│  │    hidden_size: 512                                     │    │
│  │    num_layers: 2                                        │    │
│  │    T: 3                                                 │    │
│  │    n: 6                                                 │    │
│  │    N_sup: 16                                            │    │
│  │    ema_decay: 0.999                                     │    │
│  │    retrain_interval_hours: 24                           │    │
│  │    fallback_mode: rules  # rules | keyword | disabled   │    │
│  │    loss_weights:                                        │    │
│  │      contrastive: 1.0                                   │    │
│  │      next_action: 0.5                                   │    │
│  │      outcome: 0.3                                       │    │
│  │      masked: 0.2                                        │    │
│  │                                                         │    │
│  │  # Tool Configuration                                   │    │
│  │  tools:                                                 │    │
│  │    builtin:                                             │    │
│  │      file_read: {enabled: true, max_lines: 2000}        │    │
│  │      file_write: {enabled: true, require_confirm: true} │    │
│  │      bash: {enabled: true, timeout_ms: 120000}          │    │
│  │      web_search: {enabled: true}                        │    │
│  │    mcp_servers:                                         │    │
│  │      - name: database                                   │    │
│  │        command: mcp-server-postgres                     │    │
│  │        args: ["--connection-string", "${DB_URL}"]       │    │
│  │                                                         │    │
│  │  # Training Configuration                               │    │
│  │  training:                                              │    │
│  │    auto_collect: true                                   │    │
│  │    min_episodes_for_training: 100                       │    │
│  │    train_interval_hours: 24                             │    │
│  │                                                         │    │
│  │  # Concurrency Configuration                            │    │
│  │  concurrency:                                           │    │
│  │    thread_pool_size: 4                                  │    │
│  │    max_parallel_tools: 4                                │    │
│  │    async_llm: true                                      │    │
│  │                                                         │    │
│  │  # Security Configuration                               │    │
│  │  security:                                              │    │
│  │    bash_sandbox: true                                   │    │
│  │    allowed_paths: ["${HOME}", "${CWD}"]                 │    │
│  │    blocked_commands: ["rm -rf /", "sudo"]               │    │
│  │    max_file_size_mb: 100                                │    │
│  │                                                         │    │
│  │  # Observability Configuration                          │    │
│  │  observability:                                         │    │
│  │    log_level: info  # debug, info, warn, error          │    │
│  │    log_path: ~/.kenetic/logs                            │    │
│  │    metrics_enabled: true                                │    │
│  │    metrics_port: 9090                                   │    │
│  │    trace_enabled: false                                 │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 9. Error Handling

#### 9.1 Error Handling Strategy

```
┌─────────────────────────────────────────────────────────────────┐
│                    ERROR HANDLING STRATEGY                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  PRINCIPLE: Fail fast, recover gracefully, never crash          │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 ERROR CATEGORIES                         │    │
│  │                                                         │    │
│  │  1. RETRIABLE ERRORS (auto-retry with backoff)          │    │
│  │     • LLMRateLimited → Exponential backoff (1s, 2s, 4s) │    │
│  │     • LLMConnectionFailed → Retry 3 times               │    │
│  │     • ToolTimeout → Retry once with longer timeout      │    │
│  │     • MCPConnectionFailed → Reconnect with backoff      │    │
│  │                                                         │    │
│  │  2. RECOVERABLE ERRORS (fallback behavior)              │    │
│  │     • LLMProviderUnavailable → Switch to fallback LLM   │    │
│  │     • TRMInferenceFailed → Fallback to rule-based mode  │    │
│  │     • ContextTooLarge → Trigger aggressive compaction   │    │
│  │     • CheckpointNotFound → Start fresh session          │    │
│  │                                                         │    │
│  │  3. FATAL ERRORS (stop and report)                      │    │
│  │     • LLMApiKeyMissing → Cannot proceed without key     │    │
│  │     • ConfigParseFailed → Invalid configuration         │    │
│  │     • MemoryCorrupted → Data integrity issue            │    │
│  │     • PathNotAllowed → Security violation               │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 RETRY STRATEGY                           │    │
│  │                                                         │    │
│  │  class RetryPolicy {                                    │    │
│  │      int max_retries = 3;                               │    │
│  │      Duration initial_delay = 1s;                       │    │
│  │      double backoff_multiplier = 2.0;                   │    │
│  │      Duration max_delay = 30s;                          │    │
│  │      bool add_jitter = true;  // Prevent thundering herd│    │
│  │  };                                                     │    │
│  │                                                         │    │
│  │  template<typename F>                                   │    │
│  │  auto retry_with_backoff(F&& func, RetryPolicy policy); │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 ERROR PROPAGATION                        │    │
│  │                                                         │    │
│  │  • Use Result<T, Error> for all fallible operations     │    │
│  │  • Avoid exceptions in hot paths (inference, tools)     │    │
│  │  • Log errors with context at point of occurrence       │    │
│  │  • Aggregate errors for batch operations                │    │
│  │  • Provide user-friendly error messages                 │    │
│  │                                                         │    │
│  │  Example:                                               │    │
│  │  auto result = tool_registry.execute("file_read", args);│    │
│  │  if (result.is_err()) {                                 │    │
│  │      if (result.error().is_retriable()) {               │    │
│  │          return retry_with_backoff([&] {                │    │
│  │              return tool_registry.execute(...);         │    │
│  │          });                                            │    │
│  │      } else {                                           │    │
│  │          logger.error("Tool failed: {}", result.error());│    │
│  │          return result;  // Propagate error             │    │
│  │      }                                                  │    │
│  │  }                                                      │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 10. Concurrency Model

```
┌─────────────────────────────────────────────────────────────────┐
│                    CONCURRENCY MODEL                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 THREAD ARCHITECTURE                      │    │
│  │                                                         │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │                    MAIN THREAD                     │  │    │
│  │  │  • User input handling (stdin)                    │  │    │
│  │  │  • Response output (stdout)                       │  │    │
│  │  │  • Orchestrator main loop                         │  │    │
│  │  │  • Session state management                       │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                          │                               │    │
│  │           ┌──────────────┼──────────────┐                │    │
│  │           ▼              ▼              ▼                │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐        │    │
│  │  │  LLM I/O    │ │ Tool Pool   │ │   TRM       │        │    │
│  │  │  Thread     │ │ (4 threads) │ │  Training   │        │    │
│  │  │             │ │             │ │  Thread     │        │    │
│  │  │ • Async HTTP│ │ • Parallel  │ │             │        │    │
│  │  │ • Streaming │ │   tool exec │ │ • Background│        │    │
│  │  │ • Rate limit│ │ • Sandboxed │ │   training  │        │    │
│  │  │   handling  │ │   processes │ │ • Non-block │        │    │
│  │  └─────────────┘ └─────────────┘ └─────────────┘        │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 ASYNC PATTERNS                           │    │
│  │                                                         │    │
│  │  1. LLM Streaming (non-blocking)                        │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  // C++20 coroutines for async streaming          │  │    │
│  │  │  task<Response> LLMGateway::stream(Request req) { │  │    │
│  │  │      auto stream = co_await http_client.post(...);│  │    │
│  │  │      while (auto chunk = co_await stream.next()) {│  │    │
│  │  │          co_yield parse_sse(chunk);               │  │    │
│  │  │      }                                            │  │    │
│  │  │  }                                                │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  2. Parallel Tool Execution                             │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  // Execute independent tools in parallel         │  │    │
│  │  │  std::vector<std::future<ToolResult>> futures;    │  │    │
│  │  │  for (auto& call : tool_calls) {                  │  │    │
│  │  │      if (!has_dependency(call)) {                 │  │    │
│  │  │          futures.push_back(                       │  │    │
│  │  │              pool.submit([&] {                    │  │    │
│  │  │                  return executor.execute(call);   │  │    │
│  │  │              })                                   │  │    │
│  │  │          );                                       │  │    │
│  │  │      }                                            │  │    │
│  │  │  }                                                │  │    │
│  │  │  for (auto& f : futures) results.push_back(f.get());│ │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  3. Background TRM Training                             │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  // Non-blocking training in dedicated thread     │  │    │
│  │  │  class TRMTrainer {                               │  │    │
│  │  │      std::jthread training_thread_;               │  │    │
│  │  │      std::atomic<bool> training_complete_{false}; │  │    │
│  │  │      std::atomic<float> training_progress_{0.0f}; │  │    │
│  │  │                                                   │  │    │
│  │  │      void start_training_async() {                │  │    │
│  │  │          training_thread_ = std::jthread([this] { │  │    │
│  │  │              train_model();                       │  │    │
│  │  │              training_complete_.store(true);      │  │    │
│  │  │          });                                      │  │    │
│  │  │      }                                            │  │    │
│  │  │  };                                               │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 SYNCHRONIZATION                          │    │
│  │                                                         │    │
│  │  • Memory Manager: Reader-writer lock for thread memory │    │
│  │  • Episode Buffer: Lock-free queue for concurrent writes│    │
│  │  • Tool Registry: RCU pattern for dynamic tool loading  │    │
│  │  • TRM Model: Atomic pointer swap for model updates     │    │
│  │  • Config: Read-mostly, occasional full reload          │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 11. Security

```
┌─────────────────────────────────────────────────────────────────┐
│                    SECURITY CONSIDERATIONS                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 BASH TOOL SANDBOXING                     │    │
│  │                                                         │    │
│  │  The bash tool is the highest-risk component.           │    │
│  │                                                         │    │
│  │  Sandboxing Options:                                    │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  1. Linux namespaces (nsjail/firejail)            │  │    │
│  │  │     • PID namespace (can't see other processes)   │  │    │
│  │  │     • Network namespace (optional: disable net)   │  │    │
│  │  │     • Mount namespace (restrict filesystem view)  │  │    │
│  │  │     • User namespace (run as unprivileged)        │  │    │
│  │  │                                                   │  │    │
│  │  │  2. seccomp-bpf (syscall filtering)               │  │    │
│  │  │     • Whitelist allowed syscalls                  │  │    │
│  │  │     • Block dangerous: ptrace, mount, reboot      │  │    │
│  │  │                                                   │  │    │
│  │  │  3. Resource limits (cgroups)                     │  │    │
│  │  │     • CPU time limit (prevent infinite loops)     │  │    │
│  │  │     • Memory limit (prevent OOM)                  │  │    │
│  │  │     • Process count limit                         │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  │  Command Filtering:                                     │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │  // Blocked patterns (configurable)               │  │    │
│  │  │  blocked_commands: [                              │  │    │
│  │  │    "rm -rf /",                                    │  │    │
│  │  │    "sudo",                                        │  │    │
│  │  │    "> /dev/sd*",                                  │  │    │
│  │  │    "dd if=/dev/zero",                             │  │    │
│  │  │    ":(){:|:&};:",  // Fork bomb                   │  │    │
│  │  │  ]                                                │  │    │
│  │  │                                                   │  │    │
│  │  │  // Require confirmation for                      │  │    │
│  │  │  confirm_patterns: [                              │  │    │
│  │  │    "rm -r",                                       │  │    │
│  │  │    "chmod",                                       │  │    │
│  │  │    "chown",                                       │  │    │
│  │  │  ]                                                │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 PATH VALIDATION                          │    │
│  │                                                         │    │
│  │  All file operations validate paths before execution:   │    │
│  │                                                         │    │
│  │  bool validate_path(const std::filesystem::path& p) {   │    │
│  │      // 1. Resolve to absolute path                     │    │
│  │      auto abs = std::filesystem::canonical(p);          │    │
│  │                                                         │    │
│  │      // 2. Check against allowed paths                  │    │
│  │      for (const auto& allowed : config.allowed_paths) { │    │
│  │          if (abs.string().starts_with(allowed)) {       │    │
│  │              return true;                               │    │
│  │          }                                              │    │
│  │      }                                                  │    │
│  │                                                         │    │
│  │      // 3. Block path traversal attempts                │    │
│  │      if (p.string().contains("..")) {                   │    │
│  │          return false;                                  │    │
│  │      }                                                  │    │
│  │                                                         │    │
│  │      return false;                                      │    │
│  │  }                                                      │    │
│  │                                                         │    │
│  │  Default allowed_paths:                                 │    │
│  │    • $HOME (user home directory)                        │    │
│  │    • $CWD (current working directory)                   │    │
│  │    • /tmp (temporary files)                             │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 API KEY SECURITY                         │    │
│  │                                                         │    │
│  │  • API keys read from environment variables (preferred) │    │
│  │  • Fallback: encrypted keyring (libsecret on Linux)     │    │
│  │  • Never stored in plain text config files              │    │
│  │  • Keys masked in logs: "ANTHROP****KEY"                │    │
│  │  • Memory wiped after use (secure_memzero)              │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 INPUT SANITIZATION                       │    │
│  │                                                         │    │
│  │  • Validate JSON schemas before processing              │    │
│  │  • Limit input sizes (max_file_size_mb)                 │    │
│  │  • Escape special characters in shell commands          │    │
│  │  • Validate URLs before web fetch                       │    │
│  │  • Rate limit tool calls per session                    │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### 12. Observability

```
┌─────────────────────────────────────────────────────────────────┐
│                    OBSERVABILITY                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 STRUCTURED LOGGING                       │    │
│  │                                                         │    │
│  │  Using spdlog with JSON output format:                  │    │
│  │                                                         │    │
│  │  {                                                      │    │
│  │    "timestamp": "2025-12-05T10:30:00.123Z",            │    │
│  │    "level": "info",                                     │    │
│  │    "component": "tool_registry",                        │    │
│  │    "session_id": "sess_abc123",                         │    │
│  │    "message": "Tool executed successfully",             │    │
│  │    "context": {                                         │    │
│  │      "tool": "file_read",                               │    │
│  │      "duration_ms": 45,                                 │    │
│  │      "result_size": 2048                                │    │
│  │    }                                                    │    │
│  │  }                                                      │    │
│  │                                                         │    │
│  │  Log Levels:                                            │    │
│  │  • debug: Detailed debugging (tool params, TRM scores) │    │
│  │  • info:  Normal operations (tool calls, LLM requests) │    │
│  │  • warn:  Recoverable issues (fallbacks, retries)      │    │
│  │  • error: Failures requiring attention                  │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 METRICS (Prometheus format)              │    │
│  │                                                         │    │
│  │  Agent Metrics:                                         │    │
│  │  • kenetic_agent_requests_total{status="success|fail"}  │    │
│  │  • kenetic_agent_turn_duration_seconds                  │    │
│  │  • kenetic_agent_context_tokens_used                    │    │
│  │                                                         │    │
│  │  LLM Metrics:                                           │    │
│  │  • kenetic_llm_requests_total{provider, model, status}  │    │
│  │  • kenetic_llm_latency_seconds{provider}                │    │
│  │  • kenetic_llm_tokens_input_total{provider}             │    │
│  │  • kenetic_llm_tokens_output_total{provider}            │    │
│  │  • kenetic_llm_rate_limit_retries_total                 │    │
│  │                                                         │    │
│  │  Tool Metrics:                                          │    │
│  │  • kenetic_tool_calls_total{tool, status}               │    │
│  │  • kenetic_tool_duration_seconds{tool}                  │    │
│  │  • kenetic_tool_errors_total{tool, error_code}          │    │
│  │                                                         │    │
│  │  TRM Metrics:                                           │    │
│  │  • kenetic_trm_episodes_collected                       │    │
│  │  • kenetic_trm_training_status{phase}                   │    │
│  │  • kenetic_trm_inference_duration_seconds               │    │
│  │  • kenetic_trm_loss{objective}  // Per objective loss   │    │
│  │                                                         │    │
│  │  Memory Metrics:                                        │    │
│  │  • kenetic_memory_episodes_stored                       │    │
│  │  • kenetic_memory_checkpoints_total                     │    │
│  │  • kenetic_context_compaction_total                     │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 DEBUG UTILITIES                          │    │
│  │                                                         │    │
│  │  CLI Debug Commands:                                    │    │
│  │  • /debug context   - Show current context tokens       │    │
│  │  • /debug memory    - Show memory state                 │    │
│  │  • /debug trm       - Show TRM status/training progress │    │
│  │  • /debug tools     - List active tools                 │    │
│  │  • /debug episode   - Show current episode buffer       │    │
│  │                                                         │    │
│  │  Environment Variables:                                 │    │
│  │  • KENETIC_DEBUG=1        - Enable debug mode           │    │
│  │  • KENETIC_LOG_LEVEL=debug - Verbose logging            │    │
│  │  • KENETIC_TRACE=1        - Enable tracing              │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Data Flow Diagrams

### Complete Request Flow

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           COMPLETE REQUEST FLOW                                  │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  User Input: "Fix the memory leak in session.cpp"                               │
│       │                                                                          │
│       ▼                                                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │ 1. INPUT PROCESSING                                                      │    │
│  │    • Parse user message                                                  │    │
│  │    • Extract intent: "fix bug"                                          │    │
│  │    • Identify target: "session.cpp"                                     │    │
│  └───────────────────────────────────┬─────────────────────────────────────┘    │
│                                      │                                          │
│                                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │ 2. MEMORY RETRIEVAL                                                      │    │
│  │    • Load user preferences (~/.kenetic/user.md)                         │    │
│  │    • Load project context (./KENETIC.md)                                │    │
│  │    • Search episodic memory: "memory leak fix" → 3 relevant episodes    │    │
│  │    • Get compressed history + recent 10 turns                           │    │
│  └───────────────────────────────────┬─────────────────────────────────────┘    │
│                                      │                                          │
│                                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │ 3. TOOL SELECTION (TRM-assisted)                                         │    │
│  │    • Input: task + context embeddings                                   │    │
│  │    • TRM recursion (T=3, n=6): refine tool rankings                     │    │
│  │    • Output: [file_read: 0.95, grep: 0.8, file_edit: 0.75]             │    │
│  │    • Select top-K tools for context                                     │    │
│  └───────────────────────────────────┬─────────────────────────────────────┘    │
│                                      │                                          │
│                                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │ 4. CONTEXT BUILDING                                                      │    │
│  │    • System prompt (2K tokens)                                          │    │
│  │    • User/project memory (2K tokens)                                    │    │
│  │    • Selected tool definitions (5K tokens)                              │    │
│  │    • Retrieved episodes (2K tokens)                                     │    │
│  │    • Compressed history (5K tokens)                                     │    │
│  │    • Recent turns (50K tokens)                                          │    │
│  │    • Current user message                                               │    │
│  │    ─────────────────────────────────────────                            │    │
│  │    Total: ~66K tokens (under 180K limit)                                │    │
│  └───────────────────────────────────┬─────────────────────────────────────┘    │
│                                      │                                          │
│                                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │ 5. LLM INFERENCE (Claude Opus 4.5)                                       │    │
│  │    • Send context + tools to LLM                                        │    │
│  │    • Receive response with tool calls:                                  │    │
│  │      - tool_call: file_read("src/session.cpp")                         │    │
│  └───────────────────────────────────┬─────────────────────────────────────┘    │
│                                      │                                          │
│                                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │ 6. TOOL EXECUTION                                                        │    │
│  │    • Validate parameters                                                │    │
│  │    • Execute file_read("src/session.cpp")                              │    │
│  │    • Return result (file contents)                                      │    │
│  │    • Log to episode buffer                                              │    │
│  └───────────────────────────────────┬─────────────────────────────────────┘    │
│                                      │                                          │
│                                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │ 7. CONTINUE LOOP                                                         │    │
│  │    • Add tool result to context                                         │    │
│  │    • LLM analyzes file, identifies leak at line 142                     │    │
│  │    • tool_call: file_edit(path, old_string, new_string)                │    │
│  │    • Execute edit                                                       │    │
│  │    • LLM: "Fixed the memory leak by adding destructor cleanup"         │    │
│  └───────────────────────────────────┬─────────────────────────────────────┘    │
│                                      │                                          │
│                                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │ 8. POST-PROCESSING                                                       │    │
│  │    • Display response to user                                           │    │
│  │    • Append to thread memory                                            │    │
│  │    • Compute reward (task success + efficiency)                         │    │
│  │    • Store episode for TRM training                                     │    │
│  │    • Create checkpoint (if significant)                                 │    │
│  └─────────────────────────────────────────────────────────────────────────┘    │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## API Reference (Core Interfaces)

### Memory Manager Interface

```cpp
namespace kenetic::memory {

class IMemoryManager {
public:
    virtual ~IMemoryManager() = default;

    // Session State
    virtual SessionState& get_session_state() = 0;
    virtual void save_session_state() = 0;

    // Thread Memory
    virtual void append_to_thread(const Message& msg) = 0;
    virtual std::vector<Message> get_recent_turns(int n) = 0;
    virtual std::string get_compressed_history() = 0;

    // Cross-Thread Memory
    virtual void store(const std::string& ns, const std::string& key, const json& value) = 0;
    virtual json retrieve(const std::string& ns, const std::string& key) = 0;

    // Episodic Memory
    virtual void store_episode(const Episode& episode) = 0;
    virtual std::vector<Episode> retrieve_episodes(const std::string& query, int k = 5) = 0;

    // Checkpointing
    virtual std::string create_checkpoint(const std::string& description = "") = 0;
    virtual void restore_checkpoint(const std::string& checkpoint_id) = 0;
    virtual std::vector<CheckpointInfo> list_checkpoints() = 0;

    // User/Project Memory
    virtual std::string get_user_memory() = 0;
    virtual std::string get_project_memory() = 0;
    virtual void update_user_memory(const std::string& content) = 0;
    virtual void update_project_memory(const std::string& content) = 0;
};

}  // namespace kenetic::memory
```

### LLM Gateway Interface

```cpp
namespace kenetic::llm {

class ILLMGateway {
public:
    virtual ~ILLMGateway() = default;

    // Synchronous completion
    virtual Result<Response> complete(const Request& request) = 0;

    // Streaming completion
    virtual AsyncResult<Response> stream(const Request& request) = 0;

    // Token counting
    virtual int count_tokens(const std::string& text) = 0;
    virtual int count_tokens(const std::vector<Message>& messages) = 0;

    // Embeddings (optional)
    virtual std::vector<float> embed(const std::string& text) = 0;

    // Provider info
    virtual std::string get_provider() const = 0;
    virtual std::string get_model() const = 0;
    virtual int get_max_context() const = 0;
};

}  // namespace kenetic::llm
```

### Tool Registry Interface

```cpp
namespace kenetic::tools {

class IToolRegistry {
public:
    virtual ~IToolRegistry() = default;

    // Registration
    virtual void register_tool(const ToolSpec& spec, ToolHandler handler) = 0;
    virtual void unregister_tool(const std::string& tool_id) = 0;

    // Discovery
    virtual std::vector<ToolSpec> list_tools() = 0;
    virtual std::optional<ToolSpec> get_tool(const std::string& tool_id) = 0;

    // Selection (TRM-assisted)
    virtual std::vector<RankedTool> select_tools(
        const Context& ctx,
        int max_tools = 20
    ) = 0;

    // Execution
    virtual Result<ToolResult> execute(
        const std::string& tool_id,
        const json& params
    ) = 0;

    // MCP Integration
    virtual void connect_mcp_server(const MCPServerConfig& config) = 0;
    virtual void disconnect_mcp_server(const std::string& server_name) = 0;
};

}  // namespace kenetic::tools
```

### TRM Reasoner Interface

```cpp
namespace kenetic::trm {

class ITRMReasoner {
public:
    virtual ~ITRMReasoner() = default;

    // Inference
    virtual Tensor forward(const Tensor& x, Tensor& y, Tensor& z) = 0;
    virtual Tensor deep_recursion(const Tensor& x, Tensor& y, Tensor& z) = 0;
    virtual Tensor infer(const Tensor& x, int max_supervision_steps = 16) = 0;

    // Training
    virtual void train_step(const TrainingBatch& batch) = 0;
    virtual void save(const std::string& path) = 0;
    virtual void load(const std::string& path) = 0;

    // Configuration
    virtual void set_T(int T) = 0;
    virtual void set_n(int n) = 0;
    virtual void set_N_sup(int N_sup) = 0;
};

}  // namespace kenetic::trm
```

---

## Build & Dependencies

### External Dependencies

| Dependency | Purpose | Version |
|------------|---------|---------|
| nlohmann/json | JSON parsing | 3.11+ |
| cpp-httplib | HTTP client | 0.14+ |
| llama.cpp | Local LLM inference | latest |
| SQLite | Checkpoint storage | 3.40+ |
| spdlog | Logging | 1.12+ |
| Catch2 | Testing | 3.4+ |
| libtorch (optional) | TRM training | 2.1+ |

### CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.20)
project(KeneticCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(KENETIC_BUILD_TESTS "Build tests" ON)
option(KENETIC_BUILD_TRM "Build TRM training support" ON)
option(KENETIC_USE_FAISS "Enable FAISS for vector search" OFF)

# Find dependencies
find_package(nlohmann_json REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(spdlog REQUIRED)

if(KENETIC_BUILD_TRM)
    find_package(Torch REQUIRED)
endif()

# Main library
add_library(kenetic
    src/agent/orchestrator.cpp
    src/memory/memory_manager.cpp
    src/tools/tool_registry.cpp
    src/llm/llm_gateway.cpp
    src/context/context_manager.cpp
    # ... more sources
)

target_link_libraries(kenetic
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
    spdlog::spdlog
)

if(KENETIC_BUILD_TRM)
    target_link_libraries(kenetic "${TORCH_LIBRARIES}")
endif()

# CLI executable
add_executable(kenetic-cli src/main.cpp)
target_link_libraries(kenetic-cli kenetic)

# Tests
if(KENETIC_BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
    add_executable(kenetic-tests tests/unit/test_main.cpp)
    target_link_libraries(kenetic-tests kenetic Catch2::Catch2WithMain)
endif()
```

---

## Next Steps

1. **Phase 1: Core Infrastructure**
   - [ ] Implement file-based memory manager
   - [ ] Implement LLM gateway (Claude + Gemini providers)
   - [ ] Implement basic tool registry with builtin tools

2. **Phase 2: Agent Loop**
   - [ ] Implement context builder with compaction
   - [ ] Implement main orchestrator loop
   - [ ] Add MCP server/client support

3. **Phase 3: TRM Integration**
   - [ ] Port TRM architecture to C++ (libtorch)
   - [ ] Implement tool selection optimizer
   - [ ] Add episode recording and training pipeline

4. **Phase 4: Advanced Features**
   - [ ] Multi-agent orchestration patterns
   - [ ] Streaming responses
   - [ ] Web UI / API server

---

## References

- [TRM Paper: Less is More: Recursive Reasoning with Tiny Networks](https://arxiv.org/abs/2510.04871)
- [Claude Code Memory Architecture](https://platform.claude.com/docs/en/agents-and-tools/tool-use/memory-tool)
- [LangGraph Persistence](https://langchain-ai.github.io/langgraphjs/how-tos/persistence/)
- [AWS AgentCore Memory](https://aws.amazon.com/blogs/machine-learning/amazon-bedrock-agentcore-memory-building-context-aware-agents/)
- [Google ADK Memory](https://google.github.io/adk-docs/sessions/memory/)
- [MCP Specification](https://modelcontextprotocol.io/specification/2025-06-18/server/tools)
- [Anthropic Context Engineering](https://www.anthropic.com/engineering/effective-context-engineering-for-ai-agents)
