cmake_minimum_required(VERSION 3.16)
project(ClaudeAnimationStudio VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STUDIO_BUILD_TESTS "Build the engine tests" ON)
option(STUDIO_BUILD_CLI "Build the command-line app" ON)
set(STUDIO_BUILD_APP AUTO CACHE STRING "Build the desktop app (needs Qt 6 with WebEngine): AUTO, ON or OFF")

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

# The desktop app. Built when Qt 6 Widgets and WebEngine are found (or STUDIO_BUILD_APP=ON).
if(NOT STUDIO_BUILD_APP STREQUAL "OFF")
  if(STUDIO_BUILD_APP STREQUAL "ON")
    find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets)
  else()
    find_package(Qt6 QUIET COMPONENTS Widgets WebEngineWidgets)
  endif()
  if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    add_executable(animation-studio-app WIN32 MACOSX_BUNDLE
      app/main.cpp
      app/MainWindow.cpp app/MainWindow.h
      app/StudioRun.cpp app/StudioRun.h
      app/WebChecker.cpp app/WebChecker.h
      app/History.cpp app/History.h
      app/SettingsDialog.cpp app/SettingsDialog.h
      app/Styles.h app/Theme.h
    )
    target_link_libraries(animation-studio-app PRIVATE studio_core Qt6::Widgets Qt6::WebEngineWidgets)
    set_target_properties(animation-studio-app PROPERTIES
      OUTPUT_NAME "Claude Animation Studio"
      MACOSX_BUNDLE_BUNDLE_NAME "Claude Animation Studio"
      MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.proprogramer123.animationstudio")
    message(STATUS "Building the desktop app with Qt ${Qt6_VERSION}")
  else()
    message(STATUS "Qt 6 with WebEngine not found: skipping the desktop app (the engine and CLI still build)")
  endif()
endif()
