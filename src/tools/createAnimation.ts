import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";
import { AGENTS, MODES } from "../pipeline.js";

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

export function registerCreateAnimation(server: McpServer): void {
  server.registerTool(
    "create_animation",
    {
      title: "Create animation",
      description:
        "Make a 2D or 3D animation from a prompt. A director refines the prompt, then five agents " +
        "(storyboard, designer, animator, renderer, reviewer) build the animation together.",
      inputSchema: createAnimationInput,
    },
    async ({ prompt, mode }) => {
      const job = {
        status: "accepted",
        mode,
        prompt,
        pipeline: ["director", ...AGENTS.map((a) => a.id)],
        note: "The agent pipeline is not built yet. This server currently confirms the request only.",
      };
      return {
        content: [
          {
            type: "text",
            text:
              `Request received for a ${mode.toUpperCase()} animation: "${prompt}".\n` +
              `Pipeline: ${job.pipeline.join(" -> ")}.\n` +
              job.note,
          },
        ],
        structuredContent: job,
      };
    },
  );
}
