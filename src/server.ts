import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import type { AgentRunner } from "./runners/index.js";
import { registerCreateAnimation } from "./tools/createAnimation.js";

export const SERVER_NAME = "claude-animation-studio";
export const SERVER_VERSION = "0.2.0";

export function createServer(runner?: AgentRunner): McpServer {
  const server = new McpServer({ name: SERVER_NAME, version: SERVER_VERSION });
  registerCreateAnimation(server, runner);
  return server;
}
