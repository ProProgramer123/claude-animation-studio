#include <cstdlib>
#include <stdexcept>

#include "studio/claude_code_runner.hpp"
#include "studio/mock_runner.hpp"
#include "studio/runner.hpp"

namespace studio {

std::unique_ptr<AgentRunner> createRunner(std::string name) {
  if (name.empty()) {
    const char* env = std::getenv("ANIMATION_STUDIO_RUNNER");
    name = (env && *env) ? env : "claude-code";
  }
  if (name == "claude-code") return std::make_unique<ClaudeCodeRunner>();
  if (name == "mock") return std::make_unique<MockRunner>();
  throw std::runtime_error("Unknown runner \"" + name + "\". Use \"claude-code\" or \"mock\".");
}

}  // namespace studio
