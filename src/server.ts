import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { registerCreateAnimation } from "./tools/createAnimation.js";

export const SERVER_NAME = "claude-animation-studio";
export const SERVER_VERSION = "0.1.0";

export function createServer(): McpServer {
  const server = new McpServer({ name: SERVER_NAME, version: SERVER_VERSION });
  registerCreateAnimation(server);
  return server;
}
