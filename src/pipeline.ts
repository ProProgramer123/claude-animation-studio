/**
 * The studio pipeline: a hidden director refines the prompt, then five agents
 * work in order, each handing its output to the next.
 *
 * Step 1 only describes the stages. The agents themselves arrive in Step 2.
 */

export const MODES = ["2d", "3d"] as const;
export type Mode = (typeof MODES)[number];

export interface Stage {
  id: string;
  name: string;
  role: string;
  hidden?: boolean;
}

export const DIRECTOR: Stage = {
  id: "director",
  name: "Director",
  role: "Turns the rough prompt into a detailed creative brief.",
  hidden: true,
};

export const AGENTS: readonly Stage[] = [
  { id: "storyboard", name: "Storyboard", role: "Breaks the brief into scenes and timing." },
  { id: "designer", name: "Designer", role: "Decides the characters, colors and objects." },
  { id: "animator", name: "Animator", role: "Writes the movement and keyframes." },
  { id: "renderer", name: "Renderer", role: "Turns the plan into working animation code." },
  { id: "reviewer", name: "Reviewer", role: "Checks the result and sends fixes back if something looks off." },
];
