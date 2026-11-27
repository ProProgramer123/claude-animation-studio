// Starts the built server over stdio, lists its tools and calls create_animation.
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";
import assert from "node:assert/strict";

const client = new Client({ name: "smoke-test", version: "0.0.0" });
await client.connect(new StdioClientTransport({ command: "node", args: ["dist/index.js"] }));

const { tools } = await client.listTools();
assert.ok(tools.some((t) => t.name === "create_animation"), "create_animation is listed");

const ok = await client.callTool({ name: "create_animation", arguments: { prompt: "a bouncing ball", mode: "3d" } });
assert.equal(ok.isError, undefined);
assert.equal(ok.structuredContent.mode, "3d");
assert.equal(ok.structuredContent.pipeline.length, 6);

const defaulted = await client.callTool({ name: "create_animation", arguments: { prompt: "a sunrise" } });
assert.equal(defaulted.structuredContent.mode, "2d");

const bad = await client.callTool({ name: "create_animation", arguments: { prompt: "x", mode: "4d" } });
assert.equal(bad.isError, true, "invalid mode is rejected");

await client.close();
console.log("smoke test passed");
