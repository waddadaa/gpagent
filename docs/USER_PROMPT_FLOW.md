# User Prompt Flow: Complete Process Explanation

## Overview

This document explains exactly what happens from the moment a user types a prompt until they receive a response.

---

## Visual Flow

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                                                                                  │
│   USER: "Fix the memory leak in session.cpp"                                    │
│                                                                                  │
│         │                                                                        │
│         ▼                                                                        │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐  │
│   │   STEP 1    │────►│   STEP 2    │────►│   STEP 3    │────►│   STEP 4    │  │
│   │   Input     │     │   Memory    │     │    Tool     │     │   Context   │  │
│   │  Processing │     │  Retrieval  │     │  Selection  │     │  Building   │  │
│   └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘  │
│                                                                      │          │
│         ┌────────────────────────────────────────────────────────────┘          │
│         │                                                                        │
│         ▼                                                                        │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐  │
│   │   STEP 5    │────►│   STEP 6    │────►│   STEP 7    │────►│   STEP 8    │  │
│   │     LLM     │     │    Tool     │     │   Response  │     │   Learning  │  │
│   │  Inference  │     │  Execution  │     │   Output    │     │  & Storage  │  │
│   └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘  │
│                             │                                                    │
│                             │ (loops if more tools needed)                      │
│                             └──────────────────────┐                            │
│                                                    │                            │
│                                                    ▼                            │
│                                              Back to STEP 5                     │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## Step-by-Step Breakdown

### STEP 1: Input Processing

**What happens:** Parse and understand the user's message

```
┌─────────────────────────────────────────────────────────────────┐
│                     INPUT PROCESSING                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  User Input: "Fix the memory leak in session.cpp"               │
│                                                                  │
│  1.1 RECEIVE MESSAGE                                            │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Raw text from CLI/API/UI                           │    │
│      │  + Any attached files or images                     │    │
│      │  + Metadata (timestamp, session_id)                 │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  1.2 PARSE MESSAGE                                              │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Message {                                          │    │
│      │    role: "user",                                    │    │
│      │    content: "Fix the memory leak in session.cpp",   │    │
│      │    timestamp: 1733400000,                           │    │
│      │    attachments: []                                  │    │
│      │  }                                                  │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  1.3 EXTRACT INTENT (lightweight analysis)                      │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Intent: "bug_fix"                                  │    │
│      │  Target: "session.cpp"                              │    │
│      │  Keywords: ["fix", "memory", "leak", "session"]     │    │
│      │  Complexity: "medium"                               │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  1.4 APPEND TO THREAD MEMORY                                    │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Write to: storage/sessions/sess_123/thread.jsonl   │    │
│      │  {"turn":15,"role":"user","content":"Fix..."}       │    │
│      └─────────────────────────────────────────────────────┘    │
│                                                                  │
│  Output: Parsed message + extracted intent                      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Code flow:**
```cpp
// Pseudocode
Message msg = input_handler.receive();
msg.timestamp = now();
msg.session_id = current_session.id;

Intent intent = intent_extractor.extract(msg.content);
// intent = {action: "fix", target: "session.cpp", keywords: [...]}

memory.append_to_thread(msg);
```

---

### STEP 2: Memory Retrieval

**What happens:** Gather all relevant context from memory systems

```
┌─────────────────────────────────────────────────────────────────┐
│                     MEMORY RETRIEVAL                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  2.1 LOAD USER PREFERENCES                                      │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  File: ~/.kenetic/user.md                           │    │
│      │                                                     │    │
│      │  Contents:                                          │    │
│      │  "# User Preferences                                │    │
│      │   - Prefer modern C++20                             │    │
│      │   - Use snake_case                                  │    │
│      │   - Be concise in explanations"                     │    │
│      │                                                     │    │
│      │  Tokens: ~500                                       │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  2.2 LOAD PROJECT CONTEXT                                       │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  File: ./KENETIC.md                                 │    │
│      │                                                     │    │
│      │  Contents:                                          │    │
│      │  "# Project: MyApp                                  │    │
│      │   - Build: CMake                                    │    │
│      │   - session.cpp handles user sessions               │    │
│      │   - Known issue: memory grows over time"            │    │
│      │                                                     │    │
│      │  Tokens: ~800                                       │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  2.3 SEARCH EPISODIC MEMORY                                     │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Query: "memory leak fix" + keywords from intent    │    │
│      │                                                     │    │
│      │  Search Process:                                    │    │
│      │  1. Load index: storage/episodic/index.json        │    │
│      │  2. Score each episode by:                          │    │
│      │     - Keyword match (memory, leak, fix)            │    │
│      │     - Recency (newer = higher score)               │    │
│      │     - Success rate (successful fixes preferred)    │    │
│      │  3. Return top 3 episodes                          │    │
│      │                                                     │    │
│      │  Results:                                           │    │
│      │  ┌─────────────────────────────────────────────┐   │    │
│      │  │ Episode 1: "Fixed leak in cache.cpp"        │   │    │
│      │  │   - Used RAII pattern                       │   │    │
│      │  │   - Success: true                           │   │    │
│      │  │   - Relevance: 0.85                         │   │    │
│      │  └─────────────────────────────────────────────┘   │    │
│      │  ┌─────────────────────────────────────────────┐   │    │
│      │  │ Episode 2: "Fixed leak in connection.cpp"   │   │    │
│      │  │   - Added destructor cleanup                │   │    │
│      │  │   - Success: true                           │   │    │
│      │  │   - Relevance: 0.72                         │   │    │
│      │  └─────────────────────────────────────────────┘   │    │
│      │                                                     │    │
│      │  Tokens: ~1500                                      │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  2.4 GET CONVERSATION HISTORY                                   │
│      ┌─────────────────────────────────────────────────────┐    │
│      │                                                     │    │
│      │  A. Check if compaction needed:                     │    │
│      │     Current history: 45 turns                       │    │
│      │     Threshold: 10 raw turns                         │    │
│      │     Result: Need to compress turns 1-35            │    │
│      │                                                     │    │
│      │  B. Load/generate compressed summary:               │    │
│      │     ┌─────────────────────────────────────────┐    │    │
│      │     │ "Previous conversation summary:          │    │    │
│      │     │  - User working on MyApp project        │    │    │
│      │     │  - Already fixed 2 bugs in auth module  │    │    │
│      │     │  - Tests currently passing              │    │    │
│      │     │  - User mentioned session issues earlier"│    │    │
│      │     └─────────────────────────────────────────┘    │    │
│      │     Tokens: ~2000                                   │    │
│      │                                                     │    │
│      │  C. Get recent raw turns (last 10):                │    │
│      │     Turn 36: User asked about tests                │    │
│      │     Turn 37: Agent ran tests                       │    │
│      │     Turn 38: User asked to check session.cpp       │    │
│      │     ... (full detail preserved)                    │    │
│      │     Turn 45: Current message                       │    │
│      │     Tokens: ~15000                                  │    │
│      │                                                     │    │
│      └─────────────────────────────────────────────────────┘    │
│                                                                  │
│  Output: All memory pieces ready for context building           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**File accesses:**
```
READ: ~/.kenetic/user.md
READ: ./KENETIC.md
READ: storage/episodic/index.json
READ: storage/episodic/ep_001.json (if matched)
READ: storage/episodic/ep_002.json (if matched)
READ: storage/sessions/sess_123/thread.jsonl
READ: storage/sessions/sess_123/compaction.json (if exists)
```

---

### STEP 3: Tool Selection (TRM-Assisted)

**What happens:** Use TRM to select the most relevant tools for this task

```
┌─────────────────────────────────────────────────────────────────┐
│                     TOOL SELECTION                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  3.1 GET ALL AVAILABLE TOOLS                                    │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Tool Registry contains 50+ tools:                  │    │
│      │  - file_read, file_write, file_edit                │    │
│      │  - bash, grep, glob                                 │    │
│      │  - web_search, web_fetch                           │    │
│      │  - git_status, git_commit, git_diff                │    │
│      │  - (MCP tools from connected servers)              │    │
│      │  ...                                               │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  3.2 KEYWORD PRE-FILTER (Fast)                                  │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Intent keywords: [fix, memory, leak, session, cpp]│    │
│      │                                                     │    │
│      │  Match against tool keywords:                       │    │
│      │  - file_read: [read, file, content] → match "file" │    │
│      │  - file_edit: [edit, modify, change] → relevant    │    │
│      │  - grep: [search, find, pattern] → relevant        │    │
│      │  - bash: [run, execute, command] → relevant        │    │
│      │  - web_search: [web, search] → low relevance       │    │
│      │                                                     │    │
│      │  Candidates after filter: 15 tools                 │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  3.3 TRM SCORING (Deep recursive refinement)                    │
│      ┌─────────────────────────────────────────────────────┐    │
│      │                                                     │    │
│      │  TRM Input:                                         │    │
│      │  x = embed([task_description, context_summary,      │    │
│      │             candidate_tools])                       │    │
│      │                                                     │    │
│      │  TRM Recursion (T=3, n=6):                         │    │
│      │  ┌─────────────────────────────────────────────┐   │    │
│      │  │  Initialize: y = zeros, z = zeros           │   │    │
│      │  │                                             │   │    │
│      │  │  Deep recursion 1 (no gradient):            │   │    │
│      │  │    for i in 0..6: z = net(x, y, z)         │   │    │
│      │  │    y = net(y, z)                           │   │    │
│      │  │                                             │   │    │
│      │  │  Deep recursion 2 (no gradient):            │   │    │
│      │  │    for i in 0..6: z = net(x, y, z)         │   │    │
│      │  │    y = net(y, z)                           │   │    │
│      │  │                                             │   │    │
│      │  │  Deep recursion 3 (final):                  │   │    │
│      │  │    for i in 0..6: z = net(x, y, z)         │   │    │
│      │  │    y = net(y, z)  ← Final answer           │   │    │
│      │  │                                             │   │    │
│      │  │  Total: 3 × 7 × 2 = 42 effective layers    │   │    │
│      │  └─────────────────────────────────────────────┘   │    │
│      │                                                     │    │
│      │  TRM Output (decoded y):                           │    │
│      │  ┌─────────────────────────────────────────────┐   │    │
│      │  │  Tool Rankings:                             │   │    │
│      │  │  1. file_read     → score: 0.95            │   │    │
│      │  │  2. grep          → score: 0.88            │   │    │
│      │  │  3. file_edit     → score: 0.82            │   │    │
│      │  │  4. bash          → score: 0.75            │   │    │
│      │  │  5. git_diff      → score: 0.60            │   │    │
│      │  │  ...                                        │   │    │
│      │  └─────────────────────────────────────────────┘   │    │
│      │                                                     │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  3.4 SELECT TOP-K TOOLS                                         │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Selected tools (top 8 by score):                   │    │
│      │  [file_read, grep, file_edit, bash,                │    │
│      │   git_diff, glob, file_write, git_status]          │    │
│      │                                                     │    │
│      │  These will be included in the context             │    │
│      │  Tool definitions: ~5000 tokens                    │    │
│      └─────────────────────────────────────────────────────┘    │
│                                                                  │
│  Output: Ranked list of relevant tools                          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Why TRM helps:**
- LLMs struggle when given 50+ tools (context pollution)
- TRM pre-selects the 8-10 most relevant tools
- Only 7M parameters but 42 effective layers of reasoning
- Trained on past successful tool selections

---

### STEP 4: Context Building

**What happens:** Assemble all pieces into a single prompt for the LLM

```
┌─────────────────────────────────────────────────────────────────┐
│                     CONTEXT BUILDING                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Context Budget: 200,000 tokens (Claude Opus 4.5)               │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                                                         │    │
│  │  SECTION 1: SYSTEM PROMPT (~2,000 tokens)              │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  "You are KeneticCore, an AI agent that helps   │   │    │
│  │  │   with software development tasks.              │   │    │
│  │  │                                                 │   │    │
│  │  │   Core capabilities:                            │   │    │
│  │  │   - Read, write, and edit files                │   │    │
│  │  │   - Execute bash commands                      │   │    │
│  │  │   - Search codebases                           │   │    │
│  │  │                                                 │   │    │
│  │  │   Guidelines:                                   │   │    │
│  │  │   - Always read files before editing           │   │    │
│  │  │   - Be concise and direct                      │   │    │
│  │  │   - Explain your reasoning                     │   │    │
│  │  │   ..."                                         │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  │  SECTION 2: USER PREFERENCES (~500 tokens)             │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  <user_preferences>                             │   │    │
│  │  │  - Prefer modern C++20 features                 │   │    │
│  │  │  - Use snake_case for variables                │   │    │
│  │  │  - Be concise in explanations                  │   │    │
│  │  │  </user_preferences>                           │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  │  SECTION 3: PROJECT CONTEXT (~800 tokens)              │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  <project_context>                              │   │    │
│  │  │  Project: MyApp                                 │   │    │
│  │  │  Build system: CMake                           │   │    │
│  │  │  Key files:                                     │   │    │
│  │  │  - src/session.cpp: handles user sessions      │   │    │
│  │  │  Known issues:                                  │   │    │
│  │  │  - Memory grows over time                      │   │    │
│  │  │  </project_context>                            │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  │  SECTION 4: TOOL DEFINITIONS (~5,000 tokens)           │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  <tools>                                        │   │    │
│  │  │  [                                             │   │    │
│  │  │    {                                           │   │    │
│  │  │      "name": "file_read",                      │   │    │
│  │  │      "description": "Read file contents",      │   │    │
│  │  │      "parameters": {                           │   │    │
│  │  │        "path": {"type": "string", "required"}  │   │    │
│  │  │      }                                         │   │    │
│  │  │    },                                          │   │    │
│  │  │    {                                           │   │    │
│  │  │      "name": "file_edit",                      │   │    │
│  │  │      "description": "Edit file contents",      │   │    │
│  │  │      ...                                       │   │    │
│  │  │    },                                          │   │    │
│  │  │    ... (8 tools total)                         │   │    │
│  │  │  ]                                             │   │    │
│  │  │  </tools>                                      │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  │  SECTION 5: RELEVANT PAST EXPERIENCES (~1,500 tokens)  │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  <relevant_experiences>                         │   │    │
│  │  │  Similar task: "Fixed memory leak in cache.cpp"│   │    │
│  │  │  Solution: Used RAII pattern with unique_ptr   │   │    │
│  │  │  Outcome: Successful                           │   │    │
│  │  │                                                 │   │    │
│  │  │  Similar task: "Fixed leak in connection.cpp"  │   │    │
│  │  │  Solution: Added destructor cleanup            │   │    │
│  │  │  Outcome: Successful                           │   │    │
│  │  │  </relevant_experiences>                       │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  │  SECTION 6: COMPRESSED HISTORY (~2,000 tokens)         │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  <conversation_summary>                         │   │    │
│  │  │  Earlier in this session:                       │   │    │
│  │  │  - User started working on MyApp                │   │    │
│  │  │  - Fixed 2 bugs in authentication module       │   │    │
│  │  │  - All tests currently passing                 │   │    │
│  │  │  - User mentioned session slowness at turn 12  │   │    │
│  │  │  </conversation_summary>                       │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  │  SECTION 7: RECENT HISTORY - RAW (~15,000 tokens)      │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  [Turn 36]                                      │   │    │
│  │  │  User: "Can you run the tests?"                │   │    │
│  │  │                                                 │   │    │
│  │  │  [Turn 37]                                      │   │    │
│  │  │  Assistant: "I'll run the tests now."          │   │    │
│  │  │  Tool call: bash("cmake --build . && ctest")   │   │    │
│  │  │  Result: "All 42 tests passed"                 │   │    │
│  │  │                                                 │   │    │
│  │  │  [Turn 38]                                      │   │    │
│  │  │  User: "Great! Can you check session.cpp?"     │   │    │
│  │  │  ...                                           │   │    │
│  │  │                                                 │   │    │
│  │  │  [Turn 44]                                      │   │    │
│  │  │  (previous interaction details)                │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  │  SECTION 8: CURRENT USER MESSAGE                       │    │
│  │  ┌─────────────────────────────────────────────────┐   │    │
│  │  │  [Turn 45]                                      │   │    │
│  │  │  User: "Fix the memory leak in session.cpp"    │   │    │
│  │  └─────────────────────────────────────────────────┘   │    │
│  │                                                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Total context: ~27,000 tokens (well under 200K limit)          │
│  Reserved for response: ~30,000 tokens                          │
│                                                                  │
│  Output: Complete prompt ready for LLM                          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### STEP 5: LLM Inference

**What happens:** Send context to LLM (Claude Opus 4.5) and get response

```
┌─────────────────────────────────────────────────────────────────┐
│                     LLM INFERENCE                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  5.1 SELECT PROVIDER                                            │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Context size: 27K tokens                           │    │
│      │  Task complexity: Medium (bug fix)                  │    │
│      │  User preference: Claude                            │    │
│      │                                                     │    │
│      │  Selected: Claude Opus 4.5 (claude-opus-4-5)       │    │
│      │  Max context: 200K ✓                               │    │
│      │  Supports tools: Yes ✓                             │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  5.2 SEND REQUEST                                               │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  POST https://api.anthropic.com/v1/messages        │    │
│      │                                                     │    │
│      │  {                                                  │    │
│      │    "model": "claude-opus-4-5-20251101",            │    │
│      │    "max_tokens": 8192,                             │    │
│      │    "messages": [...context...],                    │    │
│      │    "tools": [...tool_definitions...],             │    │
│      │    "tool_choice": "auto"                          │    │
│      │  }                                                  │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  5.3 LLM THINKS... (Claude's internal reasoning)               │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  "The user wants me to fix a memory leak in        │    │
│      │   session.cpp. Based on past experiences:          │    │
│      │   - RAII patterns helped with cache.cpp leak       │    │
│      │   - Destructor cleanup fixed connection.cpp        │    │
│      │                                                     │    │
│      │   First, I should read the file to understand      │    │
│      │   the current implementation and identify the      │    │
│      │   source of the leak.                              │    │
│      │                                                     │    │
│      │   I'll use file_read to examine session.cpp"       │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  5.4 RECEIVE RESPONSE                                           │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Response {                                         │    │
│      │    content: "I'll help fix the memory leak in      │    │
│      │              session.cpp. Let me first read the    │    │
│      │              file to understand the current        │    │
│      │              implementation.",                     │    │
│      │                                                     │    │
│      │    tool_calls: [                                   │    │
│      │      {                                             │    │
│      │        "id": "call_abc123",                        │    │
│      │        "name": "file_read",                        │    │
│      │        "arguments": {                              │    │
│      │          "path": "/project/src/session.cpp"       │    │
│      │        }                                           │    │
│      │      }                                             │    │
│      │    ],                                              │    │
│      │                                                     │    │
│      │    stop_reason: "tool_use",                        │    │
│      │    usage: {input: 27000, output: 150}             │    │
│      │  }                                                 │    │
│      └─────────────────────────────────────────────────────┘    │
│                                                                  │
│  Output: LLM response with tool call request                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### STEP 6: Tool Execution

**What happens:** Execute the requested tool and capture results

```
┌─────────────────────────────────────────────────────────────────┐
│                     TOOL EXECUTION                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  6.1 VALIDATE TOOL CALL                                         │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Tool: file_read                                    │    │
│      │  Arguments: {"path": "/project/src/session.cpp"}   │    │
│      │                                                     │    │
│      │  Validation:                                        │    │
│      │  ✓ Tool exists in registry                         │    │
│      │  ✓ Required parameter "path" provided              │    │
│      │  ✓ Path is absolute                                │    │
│      │  ✓ File exists                                     │    │
│      │  ✓ No confirmation required for read               │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  6.2 EXECUTE TOOL                                               │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  file_read_handler("/project/src/session.cpp")     │    │
│      │                                                     │    │
│      │  Process:                                           │    │
│      │  1. Open file                                       │    │
│      │  2. Read contents (up to 2000 lines)               │    │
│      │  3. Add line numbers                                │    │
│      │  4. Return formatted result                        │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  6.3 CAPTURE RESULT                                             │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  ToolResult {                                       │    │
│      │    tool_call_id: "call_abc123",                    │    │
│      │    success: true,                                   │    │
│      │    content: "                                       │    │
│      │      1  #include <memory>                          │    │
│      │      2  #include <vector>                          │    │
│      │      3                                             │    │
│      │      4  class Session {                            │    │
│      │      5  private:                                   │    │
│      │      6      std::vector<Connection*> connections_; │    │
│      │      7      Cache* cache_;  // RAW POINTER!        │    │
│      │      ...                                           │    │
│      │      142     void cleanup() {                      │    │
│      │      143         // BUG: connections_ not freed    │    │
│      │      144         connections_.clear();             │    │
│      │      145     }                                     │    │
│      │      ...",                                         │    │
│      │    execution_time_ms: 45                           │    │
│      │  }                                                 │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  6.4 LOG FOR TRAINING                                           │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Append to episode buffer:                          │    │
│      │  {                                                  │    │
│      │    "action": "tool_call",                          │    │
│      │    "tool": "file_read",                            │    │
│      │    "args": {"path": "..."},                        │    │
│      │    "result": "success",                            │    │
│      │    "timestamp": 1733400100                         │    │
│      │  }                                                  │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  6.5 ADD TO CONTEXT & LOOP BACK                                 │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Context now includes:                              │    │
│      │  - Previous context                                │    │
│      │  - Assistant message with tool call                │    │
│      │  - Tool result                                     │    │
│      │                                                     │    │
│      │  → Return to STEP 5 (LLM Inference)                │    │
│      │    LLM will analyze the code and decide next step │    │
│      └─────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### STEP 5-6 LOOP: Multiple Tool Calls

**What happens:** LLM continues analyzing and making changes

```
┌─────────────────────────────────────────────────────────────────┐
│                     TOOL CALL LOOP                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ITERATION 2: LLM analyzes code, finds the bug                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  LLM Response:                                          │    │
│  │  "I can see the issue. The Session class uses raw      │    │
│  │   pointers for connections_ and cache_ but doesn't     │    │
│  │   delete them in the destructor or cleanup().          │    │
│  │                                                         │    │
│  │   I'll fix this by:                                    │    │
│  │   1. Using unique_ptr instead of raw pointers          │    │
│  │   2. Properly cleaning up connections in destructor"   │    │
│  │                                                         │    │
│  │  Tool call: file_edit                                  │    │
│  │  {                                                     │    │
│  │    "path": "/project/src/session.cpp",                │    │
│  │    "old_string": "std::vector<Connection*> conn...",  │    │
│  │    "new_string": "std::vector<std::unique_ptr<..."    │    │
│  │  }                                                     │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  Execute file_edit → Success                                    │
│                              │                                   │
│                              ▼                                   │
│  ITERATION 3: LLM makes second edit                             │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Tool call: file_edit (fix destructor)                 │    │
│  │  ...                                                   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  Execute file_edit → Success                                    │
│                              │                                   │
│                              ▼                                   │
│  ITERATION 4: LLM verifies fix                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Tool call: bash                                       │    │
│  │  {"command": "cd /project && cmake --build . && ctest"}│    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  Execute bash → "All 42 tests passed"                          │
│                              │                                   │
│                              ▼                                   │
│  ITERATION 5: LLM provides final response (no more tools)      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Response:                                              │    │
│  │  "I've fixed the memory leak in session.cpp. The       │    │
│  │   issues were:                                         │    │
│  │                                                         │    │
│  │   1. Raw pointers were used for connections_ and       │    │
│  │      cache_ without proper cleanup                     │    │
│  │                                                         │    │
│  │   Changes made:                                        │    │
│  │   - Line 6: Changed to unique_ptr<Connection>         │    │
│  │   - Line 7: Changed to unique_ptr<Cache>              │    │
│  │   - Line 142-145: Added proper destructor cleanup     │    │
│  │                                                         │    │
│  │   All tests pass. The memory leak should be resolved." │    │
│  │                                                         │    │
│  │  stop_reason: "end_turn" (no more tool calls)         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Loop exits → Proceed to STEP 7                                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### STEP 7: Response Output

**What happens:** Display the final response to the user

```
┌─────────────────────────────────────────────────────────────────┐
│                     RESPONSE OUTPUT                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  7.1 FORMAT RESPONSE                                            │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Apply formatting:                                  │    │
│      │  - Markdown rendering                              │    │
│      │  - Code syntax highlighting                        │    │
│      │  - Line wrapping for terminal                      │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  7.2 DISPLAY TO USER                                            │
│      ┌─────────────────────────────────────────────────────┐    │
│      │                                                     │    │
│      │  ┌─────────────────────────────────────────────┐   │    │
│      │  │ TERMINAL OUTPUT:                            │   │    │
│      │  │                                             │   │    │
│      │  │ I've fixed the memory leak in session.cpp. │   │    │
│      │  │ The issues were:                            │   │    │
│      │  │                                             │   │    │
│      │  │ 1. Raw pointers were used for connections_  │   │    │
│      │  │    and cache_ without proper cleanup        │   │    │
│      │  │                                             │   │    │
│      │  │ Changes made:                               │   │    │
│      │  │ - Line 6: Changed to unique_ptr<Connection>│   │    │
│      │  │ - Line 7: Changed to unique_ptr<Cache>     │   │    │
│      │  │ - Line 142-145: Added destructor cleanup   │   │    │
│      │  │                                             │   │    │
│      │  │ All tests pass. Memory leak resolved.      │   │    │
│      │  │                                             │   │    │
│      │  │ >                                          │   │    │
│      │  └─────────────────────────────────────────────┘   │    │
│      │                                                     │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  7.3 APPEND TO THREAD MEMORY                                    │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Write to: storage/sessions/sess_123/thread.jsonl  │    │
│      │                                                     │    │
│      │  {"turn":46,"role":"assistant","content":"I've...", │    │
│      │   "tool_calls":[...], "timestamp":1733400200}      │    │
│      └─────────────────────────────────────────────────────┘    │
│                                                                  │
│  Output: User sees the response                                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### STEP 8: Learning & Storage

**What happens:** Record the interaction for future learning and create checkpoints

```
┌─────────────────────────────────────────────────────────────────┐
│                     LEARNING & STORAGE                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  8.1 COMPUTE REWARD SIGNALS                                     │
│      ┌─────────────────────────────────────────────────────┐    │
│      │                                                     │    │
│      │  R_task (Task Success):                            │    │
│      │  ┌───────────────────────────────────────────────┐ │    │
│      │  │ • Bug fix completed: +1.0                     │ │    │
│      │  │ • Tests pass: +0.5                            │ │    │
│      │  │ • No regressions: +0.3                        │ │    │
│      │  │ Subtotal: 1.8                                 │ │    │
│      │  └───────────────────────────────────────────────┘ │    │
│      │                                                     │    │
│      │  R_efficiency (Resource Usage):                    │    │
│      │  ┌───────────────────────────────────────────────┐ │    │
│      │  │ • Tool calls: 4 (optimal: 3-5)    → +0.8     │ │    │
│      │  │ • Tokens used: 35K (reasonable)   → +0.7     │ │    │
│      │  │ • Time taken: 45 seconds          → +0.9     │ │    │
│      │  │ Subtotal: 2.4                                 │ │    │
│      │  └───────────────────────────────────────────────┘ │    │
│      │                                                     │    │
│      │  R_quality (Output Quality):                       │    │
│      │  ┌───────────────────────────────────────────────┐ │    │
│      │  │ • Code compiles: +1.0                         │ │    │
│      │  │ • Modern C++ used: +0.5 (user preference)    │ │    │
│      │  │ • Clear explanation: +0.5                     │ │    │
│      │  │ Subtotal: 2.0                                 │ │    │
│      │  └───────────────────────────────────────────────┘ │    │
│      │                                                     │    │
│      │  Total Reward: 0.4*1.8 + 0.3*2.4 + 0.3*2.0 = 2.04 │    │
│      │                                                     │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  8.2 STORE EPISODE                                              │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Write to: storage/episodic/ep_new123.json         │    │
│      │                                                     │    │
│      │  {                                                  │    │
│      │    "episode_id": "ep_new123",                      │    │
│      │    "timestamp": 1733400200,                        │    │
│      │    "task": {                                       │    │
│      │      "description": "Fix memory leak in session",  │    │
│      │      "category": "bug_fix",                        │    │
│      │      "keywords": ["memory", "leak", "session"]    │    │
│      │    },                                              │    │
│      │    "context": {                                    │    │
│      │      "project": "MyApp",                          │    │
│      │      "files": ["src/session.cpp"]                 │    │
│      │    },                                              │    │
│      │    "trajectory": [                                 │    │
│      │      {"tool": "file_read", "result": "success"},  │    │
│      │      {"tool": "file_edit", "result": "success"},  │    │
│      │      {"tool": "file_edit", "result": "success"},  │    │
│      │      {"tool": "bash", "result": "success"}        │    │
│      │    ],                                              │    │
│      │    "outcome": {                                    │    │
│      │      "success": true,                             │    │
│      │      "turns": 5,                                  │    │
│      │      "tools_used": 4,                             │    │
│      │      "total_reward": 2.04                         │    │
│      │    },                                              │    │
│      │    "learnings": [                                  │    │
│      │      "unique_ptr fixes ownership issues",         │    │
│      │      "Always check destructors for cleanup"       │    │
│      │    ]                                               │    │
│      │  }                                                 │    │
│      │                                                     │    │
│      │  Update: storage/episodic/index.json              │    │
│      │  Add entry for quick retrieval                    │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  8.3 UPDATE TRM TRAINING BUFFER                                 │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Add to training buffer for next TRM update:       │    │
│      │                                                     │    │
│      │  Training example:                                  │    │
│      │  {                                                  │    │
│      │    input: [task_embedding, context],               │    │
│      │    output: [file_read, file_edit, file_edit, bash],│    │
│      │    reward: 2.04                                    │    │
│      │  }                                                  │    │
│      │                                                     │    │
│      │  Buffer size: 847/1000                             │    │
│      │  (Will train TRM when buffer reaches 1000)         │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  8.4 CREATE CHECKPOINT (if significant change)                  │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Checkpoint trigger: Code modification made        │    │
│      │                                                     │    │
│      │  Write to: storage/checkpoints/cp_xyz789.json     │    │
│      │  {                                                  │    │
│      │    "checkpoint_id": "cp_xyz789",                   │    │
│      │    "session_id": "sess_123",                       │    │
│      │    "timestamp": 1733400200,                        │    │
│      │    "parent": "cp_xyz788",                          │    │
│      │    "description": "Fixed memory leak",            │    │
│      │    "state": {...session_state...}                 │    │
│      │  }                                                  │    │
│      └─────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  8.5 UPDATE SESSION STATE                                       │
│      ┌─────────────────────────────────────────────────────┐    │
│      │  Write to: storage/sessions/sess_123/state.json   │    │
│      │                                                     │    │
│      │  {                                                  │    │
│      │    "conversation_turn": 46,                        │    │
│      │    "last_checkpoint": "cp_xyz789",                 │    │
│      │    "files_modified": ["src/session.cpp"],         │    │
│      │    "current_task": null  // completed             │    │
│      │  }                                                  │    │
│      └─────────────────────────────────────────────────────┘    │
│                                                                  │
│  Process complete! Ready for next user input.                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Complete Timeline

```
┌─────────────────────────────────────────────────────────────────┐
│                     TIMELINE VIEW                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  T+0ms      User types: "Fix the memory leak in session.cpp"   │
│  T+5ms      Step 1: Input processed, intent extracted          │
│  T+50ms     Step 2: Memory retrieved (user, project, episodes) │
│  T+100ms    Step 3: TRM selects 8 relevant tools               │
│  T+150ms    Step 4: Context built (27K tokens)                 │
│                                                                  │
│  T+200ms    Step 5: First LLM call sent                        │
│  T+2000ms   Response: "Let me read the file" + file_read       │
│  T+2050ms   Step 6: file_read executed                         │
│                                                                  │
│  T+2100ms   Step 5: Second LLM call (with file contents)       │
│  T+4000ms   Response: "Found bug" + file_edit                  │
│  T+4050ms   Step 6: file_edit executed                         │
│                                                                  │
│  T+4100ms   Step 5: Third LLM call                             │
│  T+5500ms   Response: "Fixing destructor" + file_edit          │
│  T+5550ms   Step 6: file_edit executed                         │
│                                                                  │
│  T+5600ms   Step 5: Fourth LLM call                            │
│  T+7000ms   Response: "Running tests" + bash                   │
│  T+10000ms  Step 6: bash executed (tests run)                  │
│                                                                  │
│  T+10050ms  Step 5: Fifth LLM call                             │
│  T+12000ms  Response: Final explanation (no tools)             │
│                                                                  │
│  T+12050ms  Step 7: Response displayed to user                 │
│  T+12100ms  Step 8: Episode stored, checkpoint created         │
│                                                                  │
│  Total time: ~12 seconds                                        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## File System Changes

```
┌─────────────────────────────────────────────────────────────────┐
│                     FILES ACCESSED/MODIFIED                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  FILES READ:                                                    │
│  ├── ~/.kenetic/user.md                    (user preferences)  │
│  ├── ./KENETIC.md                          (project context)   │
│  ├── storage/episodic/index.json          (episode index)     │
│  ├── storage/episodic/ep_001.json         (similar episode)   │
│  ├── storage/episodic/ep_002.json         (similar episode)   │
│  ├── storage/sessions/sess_123/thread.jsonl (history)         │
│  ├── storage/sessions/sess_123/state.json  (session state)    │
│  └── /project/src/session.cpp              (target file)      │
│                                                                  │
│  FILES WRITTEN:                                                 │
│  ├── /project/src/session.cpp              (bug fix)          │
│  ├── storage/sessions/sess_123/thread.jsonl (new turns)       │
│  ├── storage/sessions/sess_123/state.json  (updated state)    │
│  ├── storage/episodic/ep_new123.json      (new episode)       │
│  ├── storage/episodic/index.json          (updated index)     │
│  └── storage/checkpoints/cp_xyz789.json   (checkpoint)        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Summary

| Step | What Happens | Time | Key Output |
|------|--------------|------|------------|
| 1 | Parse user input | ~5ms | Message + Intent |
| 2 | Retrieve memories | ~50ms | Context pieces |
| 3 | TRM selects tools | ~50ms | Top 8 tools |
| 4 | Build context | ~50ms | Full prompt |
| 5 | LLM inference | ~2s | Response + tool calls |
| 6 | Execute tools | varies | Tool results |
| 5-6 | Loop until done | ~10s | Final response |
| 7 | Display output | ~50ms | User sees response |
| 8 | Learn & store | ~50ms | Episode saved |

**Total: ~12 seconds for a bug fix task with 4 tool calls**
