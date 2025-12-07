# GPAgent

A native C++/Qt AI agent with tool use, multi-provider LLM support, and self-improving tool selection.

## Features

- **Multi-Provider LLM Support**: Claude (Anthropic), Gemini (Google), with automatic fallback
- **Built-in Tools**: File operations, bash execution, grep, glob, web search, web fetch, image reading
- **Tool Selector**: Self-improving tool selection using [Tiny Recursive Models (TRM)](https://arxiv.org/abs/2510.04871) (Jolicoeur-Martineau, 2025), combining recursive reasoning with BERT-style masked prediction for unsupervised learning from usage patterns
- **Episodic Memory**: Persistent storage of task history for learning and context
- **Context Management**: Automatic summarization to handle long conversations
- **Modern Qt UI**: Clean chat interface with streaming responses

## Requirements

- C++20 compatible compiler (GCC 11+, Clang 13+, MSVC 2022+)
- CMake 3.21+
- Qt 6.5+ (Core, Quick, QuickControls2)
- OpenSSL development libraries

## Building

```bash
# Clone the repository
git clone https://github.com/waddadaa/gpagent
cd gpagent

# Create build directory
mkdir build && cd build

# Configure (adjust Qt path as needed)
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64

# Build
cmake --build . --parallel
```

### Dependencies

The following dependencies are fetched automatically via CMake FetchContent:
- [nlohmann/json](https://github.com/nlohmann/json) - MIT License
- [spdlog](https://github.com/gabime/spdlog) - MIT License
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - MIT License
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) - MIT License

## Configuration

### API Keys

Set your API keys as environment variables:

```bash
export ANTHROPIC_API_KEY="your-anthropic-key"
export GOOGLE_API_KEY="your-google-key"
export PERPLEXITY_API_KEY="your-perplexity-key"  # For web search
```

Or configure them in the Settings page within the application. Keys are stored in:
- Linux: `~/.config/gpagent/config.yaml`
- macOS: `~/Library/Preferences/gpagent/config.yaml`
- Windows: `%APPDATA%/gpagent/config.yaml`

### Runtime Data

Episodic memory, TRM models, and logs are stored in `~/.gpagent/`

## Usage

```bash
./GPAgent
```

The application provides a chat interface where you can:
- Ask questions and have conversations
- Request file operations (read, write, edit)
- Execute shell commands
- Search the web
- Analyze images

## Architecture

```
gpagent/
├── include/gpagent/     # Header files
│   ├── agent/           # Orchestrator, planner
│   ├── core/            # Config, types, errors
│   ├── llm/             # LLM providers
│   ├── memory/          # Episodic memory
│   ├── context/         # Context management
│   ├── tools/           # Tool system
│   ├── trm/             # Tool Ranking Model
│   └── ui/              # Qt UI components
├── src/                 # Implementation files
├── Qml/                 # QML UI files
└── CMakeLists.txt
```

## Tool Selector (TRM-based)

A self-improving tool selection system using [Tiny Recursive Models](https://arxiv.org/abs/2510.04871) that learns from usage patterns without requiring labeled training data.

### Architecture
- **Tiny recursive network**: 2 layers, 512 hidden size (~5-7M parameters)
- **Deep supervision**: Learns from each step, not just final outcomes
- **Recursive refinement**: Iteratively improves predictions (T=3, n=6 recursions)

### Training Objectives (Unsupervised)
| Loss | Weight | Description |
|------|--------|-------------|
| Contrastive | 1.0 | Similar tasks → similar tool choices |
| Next Action | 0.5 | Predict next tool from action sequence |
| Outcome | 0.3 | Predict task success/failure |
| Masked | 0.2 | BERT-style: mask a tool, predict from context |

### How It Works
1. **Cold Start**: Uses keyword matching + history boosting
2. **Episode Collection**: Stores task descriptions, tool sequences, and outcomes
3. **Training**: After 5+ episodes, trains on collected data
4. **Inference**: Predicts best tool with confidence scores

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Third-Party Libraries

- [Qt 6](https://www.qt.io/) - LGPL v3 (dynamically linked) or commercial license
- [nlohmann/json](https://github.com/nlohmann/json) - MIT License
- [spdlog](https://github.com/gabime/spdlog) - MIT License
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - MIT License
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) - MIT License

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
