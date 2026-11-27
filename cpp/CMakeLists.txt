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
  src/gif.cpp
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
  install(TARGETS animation-studio RUNTIME DESTINATION bin)
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
    set(APP_SOURCES
      app/main.cpp
      app/MainWindow.cpp app/MainWindow.h
      app/StudioRun.cpp app/StudioRun.h
      app/WebChecker.cpp app/WebChecker.h
      app/Exporter.cpp app/Exporter.h
      app/History.cpp app/History.h
      app/SettingsDialog.cpp app/SettingsDialog.h
      app/Styles.h app/Theme.h
      app/resources/app.qrc
    )
    set(CMAKE_AUTORCC ON)
    if(WIN32)
      list(APPEND APP_SOURCES app/resources/app.rc)
    endif()
    if(APPLE)
      # Build the .icns from the PNG with the tools that ship with macOS.
      set(ICONSET ${CMAKE_CURRENT_BINARY_DIR}/AppIcon.iconset)
      set(ICNS ${CMAKE_CURRENT_BINARY_DIR}/AppIcon.icns)
      file(MAKE_DIRECTORY ${ICONSET})
      foreach(size 16 32 128 256 512)
        math(EXPR double "${size} * 2")
        execute_process(COMMAND sips -z ${size} ${size} ${CMAKE_CURRENT_SOURCE_DIR}/app/resources/icon-512.png
                        --out ${ICONSET}/icon_${size}x${size}.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -z ${double} ${double} ${CMAKE_CURRENT_SOURCE_DIR}/app/resources/icon-512.png
                        --out ${ICONSET}/icon_${size}x${size}@2x.png OUTPUT_QUIET ERROR_QUIET)
      endforeach()
      execute_process(COMMAND iconutil -c icns ${ICONSET} -o ${ICNS} OUTPUT_QUIET ERROR_QUIET)
      if(EXISTS ${ICNS})
        list(APPEND APP_SOURCES ${ICNS})
        set_source_files_properties(${ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
      endif()
    endif()

    add_executable(animation-studio-app WIN32 MACOSX_BUNDLE ${APP_SOURCES})
    target_link_libraries(animation-studio-app PRIVATE studio_core Qt6::Widgets Qt6::WebEngineWidgets)
    set_target_properties(animation-studio-app PROPERTIES
      OUTPUT_NAME "Claude Animation Studio"
      MACOSX_BUNDLE_BUNDLE_NAME "Claude Animation Studio"
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_ICON_FILE AppIcon
      MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.proprogramer123.animationstudio")

    include(GNUInstallDirs)
    install(TARGETS animation-studio-app
      BUNDLE DESTINATION .
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(UNIX AND NOT APPLE)
      # A command name without spaces on Linux; Windows and macOS keep the friendly name.
      set_target_properties(animation-studio-app PROPERTIES OUTPUT_NAME "claude-animation-studio")
      install(FILES app/resources/claude-animation-studio.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
      install(FILES app/resources/icon-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
              RENAME claude-animation-studio.png)
    endif()
    message(STATUS "Building the desktop app with Qt ${Qt6_VERSION}")
  else()
    message(STATUS "Qt 6 with WebEngine not found: skipping the desktop app (the engine and CLI still build)")
  endif()
endif()

# Linux package: cpack -G DEB makes a .deb that pulls in Qt from the system.
set(CPACK_PACKAGE_NAME "claude-animation-studio")
set(CPACK_PACKAGE_VENDOR "Claude Animation Studio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Describe an animation and a team of AI agents builds it in 2D or 3D")
set(CPACK_PACKAGE_CONTACT "https://github.com/ProProgramer123/claude-animation-studio")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION graphics)
include(CPack)
