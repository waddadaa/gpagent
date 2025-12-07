cmake_minimum_required(VERSION 3.20)
project(GPAgent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Find Qt
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

# Find other dependencies
find_package(PkgConfig REQUIRED)

# nlohmann_json
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()

# spdlog for logging
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.13.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# cpp-httplib for HTTP client (header-only)
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

# yaml-cpp for configuration
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif()

# SQLite3 for checkpoints
find_package(SQLite3 REQUIRED)

# OpenSSL for HTTPS
find_package(OpenSSL REQUIRED)

# Poppler for PDF reading (optional)
pkg_check_modules(POPPLER poppler-cpp)
if(POPPLER_FOUND)
    add_compile_definitions(HAVE_POPPLER)
    message(STATUS "Poppler found - PDF support enabled")
else()
    message(STATUS "Poppler not found - PDF support disabled")
endif()

# Core library sources
set(GPAGENT_CORE_SOURCES
    src/core/types.cpp
    src/core/errors.cpp
    src/core/uuid.cpp
    src/core/config.cpp
)

set(GPAGENT_MEMORY_SOURCES
    src/memory/memory_manager.cpp
    src/memory/session_state.cpp
    src/memory/thread_memory.cpp
    src/memory/episodic_memory.cpp
    src/memory/checkpointer.cpp
)

set(GPAGENT_TOOLS_SOURCES
    src/tools/tool_registry.cpp
    src/tools/tool_executor.cpp
    src/tools/builtin/file_tools.cpp
    src/tools/builtin/bash_tool.cpp
    src/tools/builtin/search_tools.cpp
    src/tools/builtin/web_tools.cpp
    src/tools/builtin/git_tools.cpp
    src/tools/builtin/memory_tools.cpp
    src/tools/builtin/interaction_tools.cpp
    src/tools/builtin/code_tools.cpp
)

set(GPAGENT_LLM_SOURCES
    src/llm/llm_gateway.cpp
    src/llm/providers/claude.cpp
    src/llm/providers/gemini.cpp
    src/llm/tokenizer.cpp
)

set(GPAGENT_CONTEXT_SOURCES
    src/context/context_manager.cpp
    src/context/context_builder.cpp
    src/context/compactor.cpp
)

set(GPAGENT_TRM_SOURCES
    src/trm/trm_model.cpp
    src/trm/trm_trainer.cpp
    src/trm/episode_buffer.cpp
)

set(GPAGENT_AGENT_SOURCES
    src/agent/orchestrator.cpp
    src/agent/planner.cpp
    src/agent/executor.cpp
)

set(GPAGENT_UI_SOURCES
    src/ui/message_model.cpp
    src/ui/chat_backend.cpp
    src/ui/config_manager.cpp
    src/ui/qml_register.cpp
)

set(GPAGENT_UI_HEADERS
    include/gpagent/ui/message_model.hpp
    include/gpagent/ui/chat_backend.hpp
    include/gpagent/ui/config_manager.hpp
    include/gpagent/ui/qml_register.hpp
)

# QML resources
qt_add_resources(GPAGENT_QML_RESOURCES
    Qml/Qml.qrc
)

# Main executable
add_executable(GPAgent
    src/main.cpp
    ${GPAGENT_CORE_SOURCES}
    ${GPAGENT_MEMORY_SOURCES}
    ${GPAGENT_TOOLS_SOURCES}
    ${GPAGENT_LLM_SOURCES}
    ${GPAGENT_CONTEXT_SOURCES}
    ${GPAGENT_TRM_SOURCES}
    ${GPAGENT_AGENT_SOURCES}
    ${GPAGENT_UI_SOURCES}
    ${GPAGENT_UI_HEADERS}
    ${GPAGENT_QML_RESOURCES}
)

target_include_directories(GPAgent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(GPAgent PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    httplib::httplib
    yaml-cpp::yaml-cpp
    SQLite::SQLite3
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

# Link Poppler if available
if(POPPLER_FOUND)
    target_include_directories(GPAgent PRIVATE ${POPPLER_INCLUDE_DIRS})
    target_link_libraries(GPAgent PRIVATE ${POPPLER_LIBRARIES})
endif()

# Install
install(TARGETS GPAgent DESTINATION bin)
