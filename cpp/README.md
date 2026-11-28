# C++ engine

The studio's engine in C++17: the hidden director, the six agents, a small synthesizer, and the runners that reach
Claude. It has no dependencies beyond the standard library and the vendored single-header
[nlohmann/json](https://github.com/nlohmann/json) (MIT, in `third_party/`).

```
include/studio/
  roster.hpp              the director and the six agents
  prompts.hpp             each agent's instructions, plus 2D/3D guidance
  pipeline.hpp            runs the agents in order, with the reviewer's fix loop and an optional browser check
  runner.hpp              the AgentRunner interface and createRunner("claude-code" | "mock")
  claude_code_runner.hpp  runs each agent as a one-shot `claude -p` call on your subscription
  mock_runner.hpp         canned replies for tests and offline runs
  parse.hpp               pulls JSON and HTML out of model replies
  paths.hpp               output folder naming
  audio.hpp               turns the Sound agent's score into a looping stereo WAV (instruments, drums, effects, reverb)
  gif.hpp                 streaming animated-GIF writer with per-frame median-cut palettes
  process.hpp             runs a program with stdin/stdout pipes (POSIX and Windows)
cli/main.cpp              the animation-studio command
app/                      the Qt 6 desktop app
  MainWindow              idea box, 2D/3D, style and length, live steps, preview, history
  StudioRun               runs the pipeline on a background thread, with Stop
  WebChecker              tests each animation in Qt's built-in Chromium (errors, does it move?)
  Exporter                records the animation off screen, then writes a GIF or (with ffmpeg) an MP4
  Styles.h                style and length presets, and the "Surprise me" ideas
  History, SettingsDialog, Theme.h
tests/test_main.cpp       engine tests (ctest)
```

`runPipeline` takes an `onProgress` callback and a `cancelled` check, which the desktop app
uses to show each agent's progress live and to let you stop a run. Stopping also ends the
`claude` process that is working at that moment.

CMake builds the desktop app when it finds Qt 6 with WebEngine. Pass `-DSTUDIO_BUILD_APP=ON` to
require it or `OFF` to skip it.

Settings read from the environment, same as the MCP server: `CLAUDE_CODE_PATH`,
`ANIMATION_STUDIO_MODEL`, `ANIMATION_STUDIO_RUNNER`, `ANIMATION_STUDIO_OUTPUT_DIR`.

## Packaging

`.github/workflows/package.yml` builds a Windows installer and portable zip (windeployqt + Inno
Setup, script in `packaging/windows/`), a macOS disk image (macdeployqt, ad-hoc signed) and a
Linux `.deb` (`cpack -G DEB`). Pull requests get them as workflow artifacts; pushing a `v*` tag
attaches them to a GitHub release.
