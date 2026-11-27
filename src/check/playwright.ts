import { createRequire } from "node:module";
import { pathToFileURL } from "node:url";
import type { AnimationChecker, CheckResult } from "./types.js";

// Playwright is optional. createRequire also honours NODE_PATH, so a global install works.
const require = createRequire(import.meta.url);

type Playwright = typeof import("playwright");

function loadPlaywright(): Playwright | null {
  try {
    return require("playwright") as Playwright;
  } catch {
    return null;
  }
}

/**
 * Opens the animation in headless Chromium, collects errors, and compares two
 * screenshots a second apart to make sure something actually moves.
 * If Playwright or its browser isn't installed, the check is skipped.
 */
export class PlaywrightChecker implements AnimationChecker {
  readonly name = "playwright";
  private warned = false;

  constructor(private readonly settleMs = 2500) {}

  async check(htmlPath: string): Promise<CheckResult | null> {
    const pw = loadPlaywright();
    if (!pw) return this.skip("Playwright is not installed");

    let browser;
    try {
      browser = await pw.chromium.launch({
        executablePath: process.env.ANIMATION_STUDIO_CHROMIUM_PATH || undefined,
      });
    } catch (err) {
      return this.skip(`Chromium could not start (${(err as Error).message.split("\n")[0]})`);
    }

    try {
      const page = await browser.newPage({ viewport: { width: 960, height: 540 } });
      const errors: string[] = [];
      page.on("pageerror", (e) => errors.push(`Uncaught error: ${e.message}`));
      page.on("console", (m) => {
        if (m.type() === "error") errors.push(`Console error: ${m.text()}`);
      });
      page.on("requestfailed", (r) => errors.push(`Failed to load ${r.url()}: ${r.failure()?.errorText ?? "unknown"}`));

      await page.goto(pathToFileURL(htmlPath).href, { waitUntil: "load", timeout: 30_000 });
      await page.waitForTimeout(this.settleMs);
      const first = await page.screenshot();
      await page.waitForTimeout(1000);
      const second = await page.screenshot();

      return { errors: [...new Set(errors)], static: first.equals(second), screenshot: second };
    } finally {
      await browser.close();
    }
  }

  private skip(reason: string): null {
    if (!this.warned) {
      console.error(
        `[animation-studio] Skipping the browser check: ${reason}. ` +
          "Run `npm install playwright && npx playwright install chromium` to enable it.",
      );
      this.warned = true;
    }
    return null;
  }
}

/** Picks the checker from ANIMATION_STUDIO_BROWSER_CHECK ("on" by default, "off" to disable). */
export function createChecker(setting = process.env.ANIMATION_STUDIO_BROWSER_CHECK ?? "on"): AnimationChecker | undefined {
  return setting === "off" ? undefined : new PlaywrightChecker();
}
