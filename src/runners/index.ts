import { ClaudeCodeRunner } from "./claudeCode.js";
import { MockRunner } from "./mock.js";
import type { AgentRunner } from "./types.js";

export type { AgentRunner, AgentRequest } from "./types.js";

/**
 * Picks the backend from ANIMATION_STUDIO_RUNNER.
 * "claude-code" (default) uses the person's Claude Code subscription.
 * "mock" returns canned replies, for tests.
 * An API-key backend can be added here later without changing any agent.
 */
export function createRunner(name = process.env.ANIMATION_STUDIO_RUNNER ?? "claude-code"): AgentRunner {
  switch (name) {
    case "claude-code":
      return new ClaudeCodeRunner();
    case "mock":
      return new MockRunner();
    default:
      throw new Error(`Unknown ANIMATION_STUDIO_RUNNER "${name}". Use "claude-code" or "mock".`);
  }
}
