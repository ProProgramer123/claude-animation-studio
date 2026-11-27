#include "studio/claude_code_runner.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "studio/process.hpp"

namespace studio {
namespace {

std::string envOr(const char* name, const std::string& fallback) {
  const char* v = std::getenv(name);
  return (v && *v) ? std::string(v) : fallback;
}

std::string trimmed(std::string s, size_t max) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  s = s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
  if (s.size() > max) s.resize(max);
  return s;
}

/** A file holding `content` that is deleted when it goes out of scope. */
struct TempFile {
  std::filesystem::path path;
  explicit TempFile(const std::string& content) {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path = std::filesystem::temp_directory_path() /
           ("animation-studio-" + std::to_string(stamp) + "-" + std::to_string(counter++) + ".txt");
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Could not write " + path.string());
    f << content;
  }
  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;
};

}  // namespace

ClaudeCodeRunner::ClaudeCodeRunner(ClaudeCodeRunnerOptions options) : options_(std::move(options)) {
  if (options_.command.empty()) options_.command = envOr("CLAUDE_CODE_PATH", "claude");
  if (options_.model.empty()) options_.model = envOr("ANIMATION_STUDIO_MODEL", "");
}

std::vector<std::string> ClaudeCodeRunner::arguments(const std::string& systemPromptFile) const {
  std::vector<std::string> args{
      "-p", "--output-format", "json", "--system-prompt-file", systemPromptFile,
      "--tools", "", "--strict-mcp-config", "--no-session-persistence",
  };
  if (!options_.model.empty()) {
    args.push_back("--model");
    args.push_back(options_.model);
  }
  return args;
}

std::string ClaudeCodeRunner::run(const AgentRequest& request) {
  const std::string agent(stage(request.agent).key);
  // The system prompt goes in a file and the work on stdin, so nothing long or full of quotes
  // passes through the command line (claude is a .cmd script on some Windows installs).
  const TempFile system(request.system);
  const ProcessResult r =
      runProcess(options_.command, arguments(system.path.string()), request.prompt, options_.timeout);
  if (!r.started) {
    throw std::runtime_error("Could not start Claude Code (\"" + options_.command +
                             "\"). Is it installed and on your PATH? " + r.startError);
  }
  if (r.timedOut) {
    throw std::runtime_error(agent + " agent timed out after " +
                             std::to_string(options_.timeout.count() / 1000) + "s");
  }
  return parseClaudeReply(agent, r.exitCode, r.out, r.err);
}

std::string parseClaudeReply(std::string_view agent, int exitCode, const std::string& out, const std::string& err) {
  nlohmann::json parsed = nlohmann::json::parse(out, nullptr, /*allow_exceptions=*/false);
  const bool ok = exitCode == 0 && parsed.is_object() && !parsed.value("is_error", false) &&
                  parsed.contains("result") && parsed["result"].is_string();
  if (ok) return parsed["result"].get<std::string>();

  std::string detail;
  if (parsed.is_object() && parsed.contains("result") && parsed["result"].is_string())
    detail = parsed["result"].get<std::string>();
  else
    detail = !err.empty() ? err : out;
  detail = trimmed(detail, 500);
  throw std::runtime_error(std::string(agent) + " agent failed (exit " + std::to_string(exitCode) +
                           "): " + (detail.empty() ? "no output" : detail));
}

}  // namespace studio
