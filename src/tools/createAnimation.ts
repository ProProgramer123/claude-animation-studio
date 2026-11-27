import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { pathToFileURL } from "node:url";
import { z } from "zod";
import { runPipeline } from "../agents/pipeline.js";
import { MODES } from "../agents/roster.js";
import { createChecker } from "../check/playwright.js";
import type { AnimationChecker } from "../check/types.js";
import { createRunner, type AgentRunner } from "../runners/index.js";
import { animationDir } from "../util/paths.js";

export const createAnimationInput = {
  prompt: z
    .string()
    .trim()
    .min(1, "Describe the animation you want.")
    .max(4000)
    .describe("What the animation should show, in your own words."),
  mode: z
    .enum(MODES)
    .default("2d")
    .describe('"2d" for an SVG/canvas animation, "3d" for a Three.js scene.'),
};

export function registerCreateAnimation(
  server: McpServer,
  runner: AgentRunner = createRunner(),
  checker: AnimationChecker | undefined = createChecker(),
): void {
  server.registerTool(
    "create_animation",
    {
      title: "Create animation",
      description:
        "Make a 2D or 3D animation from a prompt. A director refines the prompt, then five agents " +
        "(storyboard, designer, animator, renderer, reviewer) build it together. Writes a self-contained " +
        "HTML file and returns its path. Takes a few minutes.",
      inputSchema: createAnimationInput,
    },
    async ({ prompt, mode }, extra) => {
      const progressToken = extra._meta?.progressToken;
      let step = 0;
      try {
        const result = await runPipeline({
          prompt,
          mode,
          runner,
          checker,
          outputDir: animationDir(prompt),
          onProgress: async (_stage, message) => {
            step += 1;
            console.error(`[animation-studio] ${message}`);
            if (progressToken !== undefined) {
              await extra.sendNotification({
                method: "notifications/progress",
                params: { progressToken, progress: step, message },
              });
            }
          },
        });
        const status = result.approved
          ? `Reviewer approved it${result.revisions ? ` after ${result.revisions} revision(s)` : ""}.`
          : `Reviewer still had concerns after ${result.revisions} revision(s): ${result.openIssues.join("; ")}`;
        return {
          content: [
            {
              type: "text",
              text:
                `Made a ${mode.toUpperCase()} animation: "${result.title}".\n` +
                `Open it: ${pathToFileURL(result.htmlPath).href}\n` +
                `All agent work is saved in ${result.outputDir}\n` +
                (result.previewPath ? `Preview screenshot: ${result.previewPath}\n` : "") +
                status,
            },
          ],
          structuredContent: { ...result },
        };
      } catch (err) {
        return {
          isError: true,
          content: [{ type: "text", text: `The studio could not finish: ${(err as Error).message}` }],
        };
      }
    },
  );
}
