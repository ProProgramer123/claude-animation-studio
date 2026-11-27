import type { AgentRequest, AgentRunner } from "./types.js";

/**
 * A runner that returns canned replies without calling any model.
 * Used by tests and handy for trying the pipeline offline.
 */
export class MockRunner implements AgentRunner {
  readonly name = "mock";
  readonly calls: AgentRequest[] = [];
  private reviewCount = 0;

  /** @param rejectFirstReviews how many reviews should ask for fixes before approving */
  constructor(private readonly rejectFirstReviews = 0) {}

  async run(request: AgentRequest): Promise<string> {
    this.calls.push(request);
    const is3d = request.prompt.includes("Mode: 3D");
    switch (request.agent) {
      case "director":
        return "# Brief\nA red ball bounces three times across a soft blue background, then rests.";
      case "storyboard":
        return JSON.stringify({
          title: "Bouncing ball",
          duration_seconds: 4,
          scenes: [{ id: "s1", start: 0, end: 4, description: "Ball bounces left to right" }],
        });
      case "designer":
        return JSON.stringify({
          background: "#cfe8ff",
          palette: ["#e63946", "#cfe8ff"],
          elements: [{ id: "ball", name: "Ball", description: "Glossy red ball", colors: ["#e63946"] }],
        });
      case "animator":
        return JSON.stringify({
          loop: true,
          tracks: [
            {
              element: "ball",
              keyframes: [
                { t: 0, props: { x: 0, y: 0 } },
                { t: 2, props: { x: 50, y: 100 } },
                { t: 4, props: { x: 100, y: 0 } },
              ],
            },
          ],
        });
      case "renderer":
        return is3d
          ? '```html\n<!doctype html><html><body><script type="module">import * as THREE from "three";</script></body></html>\n```'
          : '```html\n<!doctype html><html><body><svg viewBox="0 0 100 100"><circle r="5" fill="#e63946"/></svg></body></html>\n```';
      case "reviewer": {
        this.reviewCount += 1;
        const approved = this.reviewCount > this.rejectFirstReviews;
        return JSON.stringify(
          approved ? { approved: true, issues: [] } : { approved: false, issues: ["Ball never touches the ground."] },
        );
      }
      default:
        throw new Error(`MockRunner has no reply for agent "${request.agent}"`);
    }
  }
}
