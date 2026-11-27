#pragma once

#include <memory>
#include <string>

#include "studio/roster.hpp"

namespace studio {

/** One agent's turn: its rules plus the work it is given. */
struct AgentRequest {
  StageId agent;
  std::string system;  // the agent's role and rules
  std::string prompt;  // the brief plus earlier agents' output
};

/** Sends one agent's request to a model and returns its reply.
 *  The pipeline only talks to this interface, so the backend can be swapped
 *  (Claude Code subscription today, an API key later) without touching any agent.
 *  Implementations throw std::runtime_error on failure. */
class AgentRunner {
 public:
  virtual ~AgentRunner() = default;
  virtual std::string name() const = 0;
  virtual std::string run(const AgentRequest& request) = 0;
};

/** Picks a backend by name: "claude-code" (default) or "mock".
 *  An empty name reads $ANIMATION_STUDIO_RUNNER. Throws for unknown names. */
std::unique_ptr<AgentRunner> createRunner(std::string name = {});

}  // namespace studio
