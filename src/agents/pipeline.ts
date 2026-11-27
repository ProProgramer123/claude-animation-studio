import { mkdir, writeFile } from "node:fs/promises";
import path from "node:path";
import type { AgentRunner } from "../runners/index.js";
import { extractHtml, extractJson } from "../util/parse.js";
import { modeNote, SYSTEM_PROMPTS } from "./prompts.js";
import type { Mode, StageId } from "./roster.js";

export interface Review {
  approved: boolean;
  issues: string[];
}

export interface PipelineOptions {
  prompt: string;
  mode: Mode;
  runner: AgentRunner;
  /** Folder where this animation's files are written. */
  outputDir: string;
  /** How many times the reviewer may send work back to the renderer. */
  maxRevisions?: number;
  onProgress?: (stage: StageId, message: string) => void | Promise<void>;
}

export interface PipelineResult {
  title: string;
  mode: Mode;
  outputDir: string;
  htmlPath: string;
  approved: boolean;
  revisions: number;
  openIssues: string[];
}

/**
 * Runs the studio: hidden director -> storyboard -> designer -> animator -> renderer -> reviewer.
 * If the reviewer finds problems, the renderer gets the issues and tries again.
 * Every stage's output is saved next to the final index.html.
 */
export async function runPipeline(options: PipelineOptions): Promise<PipelineResult> {
  const { prompt, mode, runner, outputDir, onProgress } = options;
  const maxRevisions = options.maxRevisions ?? 2;
  await mkdir(outputDir, { recursive: true });

  const save = (name: string, content: unknown) =>
    writeFile(
      path.join(outputDir, name),
      typeof content === "string" ? content : JSON.stringify(content, null, 2) + "\n",
    );

  const ask = async (agent: StageId, context: string, label: string) => {
    await onProgress?.(agent, label);
    return runner.run({ agent, system: SYSTEM_PROMPTS[agent], prompt: `${modeNote(mode)}\n\n${context}` });
  };

  await save("prompt.txt", prompt + "\n");

  const brief = (await ask("director", `Client's idea:\n${prompt}`, "Director is refining the prompt")).trim();
  await save("brief.md", brief + "\n");

  const storyboard = extractJson<{ title?: string }>(
    await ask("storyboard", `Brief:\n${brief}`, "Storyboard is planning scenes and timing"),
    "storyboard",
  );
  await save("storyboard.json", storyboard);

  const design = extractJson(
    await ask(
      "designer",
      `Brief:\n${brief}\n\nStoryboard:\n${JSON.stringify(storyboard)}`,
      "Designer is choosing characters, colors and objects",
    ),
    "designer",
  );
  await save("design.json", design);

  const animation = extractJson(
    await ask(
      "animator",
      `Brief:\n${brief}\n\nStoryboard:\n${JSON.stringify(storyboard)}\n\nDesign:\n${JSON.stringify(design)}`,
      "Animator is writing keyframes",
    ),
    "animator",
  );
  await save("animation.json", animation);

  const plan =
    `Brief:\n${brief}\n\nStoryboard:\n${JSON.stringify(storyboard)}\n\n` +
    `Design:\n${JSON.stringify(design)}\n\nKeyframes:\n${JSON.stringify(animation)}`;

  let html = extractHtml(await ask("renderer", plan, "Renderer is building the animation"));
  let review: Review = { approved: false, issues: [] };
  let revisions = 0;
  const reviews: Review[] = [];

  for (;;) {
    review = normalizeReview(
      extractJson(
        await ask("reviewer", `${plan}\n\nHTML:\n${html}`, "Reviewer is checking the result"),
        "reviewer",
      ),
    );
    reviews.push(review);
    if (review.approved || revisions >= maxRevisions) break;

    revisions += 1;
    html = extractHtml(
      await ask(
        "renderer",
        `${plan}\n\nYour previous HTML:\n${html}\n\nThe reviewer asked for these fixes:\n` +
          review.issues.map((i) => `- ${i}`).join("\n") +
          "\n\nReturn the full corrected HTML.",
        `Renderer is applying fixes (revision ${revisions})`,
      ),
    );
  }

  const htmlPath = path.join(outputDir, "index.html");
  await save("index.html", html);
  await save("review.json", { revisions, reviews });

  return {
    title: storyboard.title ?? "Untitled animation",
    mode,
    outputDir,
    htmlPath,
    approved: review.approved,
    revisions,
    openIssues: review.approved ? [] : review.issues,
  };
}

function normalizeReview(raw: unknown): Review {
  const r = (raw ?? {}) as Partial<Review>;
  return {
    approved: r.approved === true,
    issues: Array.isArray(r.issues) ? r.issues.map(String) : [],
  };
}
