import path from "node:path";

/** Folder for a new animation: <base>/<timestamp>-<slug>. */
export function animationDir(prompt: string, base = process.env.ANIMATION_STUDIO_OUTPUT_DIR ?? "animations"): string {
  const slug =
    prompt
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, "-")
      .replace(/^-+|-+$/g, "")
      .slice(0, 40) || "animation";
  const stamp = new Date().toISOString().replace(/[:.]/g, "-").slice(0, 19);
  return path.resolve(base, `${stamp}-${slug}`);
}
