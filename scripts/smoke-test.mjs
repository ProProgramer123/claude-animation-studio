// Starts the built server over stdio (with the mock runner), lists its tools and calls create_animation.
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";

const outputDir = mkdtempSync(path.join(tmpdir(), "studio-smoke-"));
const client = new Client({ name: "smoke-test", version: "0.0.0" });
await client.connect(
  new StdioClientTransport({
    command: "node",
    args: ["dist/index.js"],
    env: { ...process.env, ANIMATION_STUDIO_RUNNER: "mock", ANIMATION_STUDIO_OUTPUT_DIR: outputDir },
    stderr: "ignore",
  }),
);

const { tools } = await client.listTools();
assert.ok(tools.some((t) => t.name === "create_animation"), "create_animation is listed");

const progress = [];
const ok = await client.callTool(
  { name: "create_animation", arguments: { prompt: "a bouncing ball", mode: "3d" } },
  undefined,
  { onprogress: (p) => progress.push(p.message) },
);
assert.equal(ok.isError, undefined, JSON.stringify(ok.content));
assert.equal(ok.structuredContent.mode, "3d");
assert.equal(ok.structuredContent.approved, true);
assert.ok(existsSync(ok.structuredContent.htmlPath), "index.html was written");
assert.equal(progress.length, 6, `progress updates: ${progress.join(" | ")}`);

const defaulted = await client.callTool({ name: "create_animation", arguments: { prompt: "a sunrise" } });
assert.equal(defaulted.structuredContent.mode, "2d");

const bad = await client.callTool({ name: "create_animation", arguments: { prompt: "x", mode: "4d" } });
assert.equal(bad.isError, true, "invalid mode is rejected");

await client.close();
console.log("smoke test passed");
