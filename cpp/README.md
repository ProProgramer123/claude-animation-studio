# C++ engine

The studio's engine in C++17: the hidden director, the five agents, and the runners that reach
Claude. It has no dependencies beyond the standard library and the vendored single-header
[nlohmann/json](https://github.com/nlohmann/json) (MIT, in `third_party/`).

```
include/studio/
  roster.hpp              the director and the five agents
  prompts.hpp             each agent's instructions, plus 2D/3D guidance
  pipeline.hpp            runs the agents in order, with the reviewer's fix loop and an optional browser check
  runner.hpp              the AgentRunner interface and createRunner("claude-code" | "mock")
  claude_code_runner.hpp  runs each agent as a one-shot `claude -p` call on your subscription
  mock_runner.hpp         canned replies for tests and offline runs
  parse.hpp               pulls JSON and HTML out of model replies
  paths.hpp               output folder naming
  process.hpp             runs a program with stdin/stdout pipes (POSIX and Windows)
cli/main.cpp              the animation-studio command
tests/test_main.cpp       engine tests (ctest)
```

`runPipeline` takes an `onProgress` callback and a `cancelled` check, which the desktop app
uses to show each agent's progress live and to let you stop a run.

Settings read from the environment, same as the MCP server: `CLAUDE_CODE_PATH`,
`ANIMATION_STUDIO_MODEL`, `ANIMATION_STUDIO_RUNNER`, `ANIMATION_STUDIO_OUTPUT_DIR`.
