/**
 * An AgentRunner sends one agent's instructions to a model and returns its reply.
 * The pipeline only talks to this interface, so the backend can be swapped
 * (Claude Code subscription today, an API-key backend later) without touching agents.
 */
export interface AgentRequest {
  /** Which stage is asking, e.g. "storyboard". Used for logs and by test runners. */
  agent: string;
  /** The agent's role and rules. */
  system: string;
  /** The work for this turn: the brief plus earlier agents' output. */
  prompt: string;
}

export interface AgentRunner {
  readonly name: string;
  run(request: AgentRequest): Promise<string>;
}
