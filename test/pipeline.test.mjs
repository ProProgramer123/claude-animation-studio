import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, readFile, readdir } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { runPipeline } from "../dist/agents/pipeline.js";
import { MockRunner } from "../dist/runners/mock.js";

const tmp = () => mkdtemp(path.join(tmpdir(), "studio-"));

test("runs director then the five agents in order and saves every stage", async () => {
  const runner = new MockRunner();
  const outputDir = await tmp();
  const result = await runPipeline({ prompt: "a bouncing ball", mode: "2d", runner, outputDir });

  assert.deepEqual(
    runner.calls.map((c) => c.agent),
    ["director", "storyboard", "designer", "animator", "renderer", "reviewer"],
  );
  assert.equal(result.approved, true);
  assert.equal(result.revisions, 0);
  assert.equal(result.title, "Bouncing ball");
  const files = (await readdir(outputDir)).sort();
  assert.deepEqual(files, [
    "animation.json", "brief.md", "design.json", "index.html", "prompt.txt", "review.json", "storyboard.json",
  ]);
  assert.match(await readFile(result.htmlPath, "utf8"), /^<!doctype html>/i);
});

test("each agent sees the director's brief, not just the raw prompt", async () => {
  const runner = new MockRunner();
  await runPipeline({ prompt: "a bouncing ball", mode: "2d", runner, outputDir: await tmp() });
  for (const call of runner.calls.slice(1)) {
    assert.match(call.prompt, /A red ball bounces three times/);
  }
});

test("mode guidance reaches every agent", async () => {
  const runner = new MockRunner();
  await runPipeline({ prompt: "a spinning cube", mode: "3d", runner, outputDir: await tmp() });
  assert.ok(runner.calls.every((c) => c.prompt.startsWith("Mode: 3D")));
});

test("reviewer issues go back to the renderer until approved", async () => {
  const runner = new MockRunner(1);
  const result = await runPipeline({ prompt: "a bouncing ball", mode: "2d", runner, outputDir: await tmp() });
  assert.deepEqual(
    runner.calls.slice(4).map((c) => c.agent),
    ["renderer", "reviewer", "renderer", "reviewer"],
  );
  assert.match(runner.calls[6].prompt, /Ball never touches the ground/);
  assert.equal(result.approved, true);
  assert.equal(result.revisions, 1);
});

test("stops after maxRevisions and reports open issues", async () => {
  const runner = new MockRunner(10);
  const result = await runPipeline({
    prompt: "a bouncing ball", mode: "2d", runner, outputDir: await tmp(), maxRevisions: 2,
  });
  assert.equal(result.approved, false);
  assert.equal(result.revisions, 2);
  assert.deepEqual(result.openIssues, ["Ball never touches the ground."]);
});

test("a stage that returns broken JSON fails with the agent's name", async () => {
  const runner = { name: "broken", run: async ({ agent }) => (agent === "director" ? "brief" : "not json") };
  await assert.rejects(
    runPipeline({ prompt: "x", mode: "2d", runner, outputDir: await tmp() }),
    /storyboard agent did not return JSON/,
  );
});
