#!/usr/bin/env node
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { createServer } from "./server.js";

async function main(): Promise<void> {
  const server = createServer();
  await server.connect(new StdioServerTransport());
  // stdout carries the MCP protocol, so log to stderr only.
  console.error("claude-animation-studio MCP server running on stdio");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
