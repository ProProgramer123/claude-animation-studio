#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "studio/runner.hpp"

namespace studio {

struct ClaudeCodeRunnerOptions {
  /** Path to the Claude Code CLI. Empty: $CLAUDE_CODE_PATH, then `claude` on PATH. */
  std::string command;
  /** Model alias or id for --model. Empty: $ANIMATION_STUDIO_MODEL, then the person's Claude Code setting. */
  std::string model;
  std::chrono::milliseconds timeout{10 * 60 * 1000};
  /** Polled while an agent runs; returning true stops it at once (run() then throws). */
  std::function<bool()> cancelled;
};

/** Runs each agent as a one-shot `claude -p` call, so it uses the person's own
 *  Claude Code login and subscription. No API key is needed.
 *  Agents get no tools and no MCP servers: they only read the prompt and write text. */
class ClaudeCodeRunner : public AgentRunner {
 public:
  explicit ClaudeCodeRunner(ClaudeCodeRunnerOptions options = {});
  std::string name() const override { return "claude-code"; }
  std::string run(const AgentRequest& request) override;

  /** The arguments passed to the CLI (exposed for tests). */
  std::vector<std::string> arguments(const std::string& systemPromptFile) const;

 private:
  ClaudeCodeRunnerOptions options_;
};

/** Reads the `claude -p --output-format json` reply. Throws with a readable message on failure. */
std::string parseClaudeReply(std::string_view agent, int exitCode, const std::string& out, const std::string& err);

}  // namespace studio
