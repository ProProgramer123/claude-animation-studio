import { spawn } from "node:child_process";
import type { AgentRequest, AgentRunner } from "./types.js";

export interface ClaudeCodeRunnerOptions {
  /** Path to the Claude Code CLI. Defaults to `claude` on PATH. */
  command?: string;
  /** Optional model alias or id passed to `--model`. Defaults to the user's Claude Code setting. */
  model?: string;
  /** Per-agent timeout in milliseconds. */
  timeoutMs?: number;
}

/**
 * Runs each agent as a one-shot `claude -p` call, so it uses the person's own
 * Claude Code login and subscription. No API key is needed.
 *
 * Agents get no tools and no MCP servers: they only read the prompt and write text.
 */
export class ClaudeCodeRunner implements AgentRunner {
  readonly name = "claude-code";
  private readonly command: string;
  private readonly model?: string;
  private readonly timeoutMs: number;

  constructor(options: ClaudeCodeRunnerOptions = {}) {
    this.command = options.command ?? process.env.CLAUDE_CODE_PATH ?? "claude";
    this.model = options.model ?? process.env.ANIMATION_STUDIO_MODEL;
    this.timeoutMs = options.timeoutMs ?? 10 * 60 * 1000;
  }

  run({ agent, system, prompt }: AgentRequest): Promise<string> {
    const args = [
      "-p",
      "--output-format", "json",
      "--system-prompt", system,
      "--tools", "",
      "--strict-mcp-config",
      "--no-session-persistence",
    ];
    if (this.model) args.push("--model", this.model);

    return new Promise((resolve, reject) => {
      const child = spawn(this.command, args, { stdio: ["pipe", "pipe", "pipe"] });
      let stdout = "";
      let stderr = "";
      const timer = setTimeout(() => {
        child.kill("SIGTERM");
        reject(new Error(`${agent} agent timed out after ${Math.round(this.timeoutMs / 1000)}s`));
      }, this.timeoutMs);

      child.stdout.on("data", (d) => (stdout += d));
      child.stderr.on("data", (d) => (stderr += d));
      child.on("error", (err) => {
        clearTimeout(timer);
        reject(
          new Error(
            `Could not start Claude Code ("${this.command}"). Is it installed and on your PATH? ${err.message}`,
          ),
        );
      });
      child.on("close", (code) => {
        clearTimeout(timer);
        let parsed: { result?: string; is_error?: boolean } | undefined;
        try {
          parsed = JSON.parse(stdout);
        } catch {
          /* fall through */
        }
        if (code !== 0 || !parsed || parsed.is_error || typeof parsed.result !== "string") {
          const detail = (parsed?.result ?? stderr ?? stdout).toString().trim().slice(0, 500);
          reject(new Error(`${agent} agent failed (exit ${code}): ${detail || "no output"}`));
          return;
        }
        resolve(parsed.result);
      });

      // Send the prompt on stdin so long briefs never hit argument-length limits.
      child.stdin.end(prompt);
    });
  }
}
