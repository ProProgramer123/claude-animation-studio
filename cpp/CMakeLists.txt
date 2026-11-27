cmake_minimum_required(VERSION 3.16)
project(ClaudeAnimationStudio VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STUDIO_BUILD_TESTS "Build the engine tests" ON)
option(STUDIO_BUILD_CLI "Build the command-line app" ON)

# The engine: director + five agents, runners and parsing. No dependencies beyond the
# standard library and the vendored single-header nlohmann/json.
add_library(studio_core
  src/roster.cpp
  src/prompts.cpp
  src/parse.cpp
  src/paths.cpp
  src/process.cpp
  src/claude_code_runner.cpp
  src/mock_runner.cpp
  src/runner_factory.cpp
  src/pipeline.cpp
)
target_include_directories(studio_core
  PUBLIC include
  PUBLIC third_party
)
if(MSVC)
  target_compile_options(studio_core PRIVATE /W4 /utf-8)
else()
  target_compile_options(studio_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(STUDIO_BUILD_CLI)
  add_executable(animation-studio cli/main.cpp)
  target_link_libraries(animation-studio PRIVATE studio_core)
endif()

if(STUDIO_BUILD_TESTS)
  enable_testing()
  add_executable(studio_tests tests/test_main.cpp)
  target_link_libraries(studio_tests PRIVATE studio_core)
  add_test(NAME studio_tests COMMAND studio_tests)
endif()
