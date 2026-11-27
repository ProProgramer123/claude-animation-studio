import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, writeFile } from "node:fs/promises";
import { createRequire } from "node:module";
import { tmpdir } from "node:os";
import path from "node:path";
import { PlaywrightChecker } from "../dist/check/playwright.js";

let hasBrowser = false;
try {
  const { chromium } = createRequire(import.meta.url)("playwright");
  await (await chromium.launch()).close();
  hasBrowser = true;
} catch {}
const opts = { skip: hasBrowser ? false : "Playwright Chromium is not installed" };

async function page(html) {
  const file = path.join(await mkdtemp(path.join(tmpdir(), "check-")), "index.html");
  await writeFile(file, html);
  return file;
}

test("sees a moving, error-free animation", opts, async () => {
  const file = await page(`<!doctype html><html><body style="margin:0"><canvas id=c width=200 height=200></canvas><script>
    const x = c.getContext("2d"); (function f(t){ x.fillStyle="#fff"; x.fillRect(0,0,200,200);
    x.fillStyle="red"; x.fillRect((t/10)%180,50,20,20); requestAnimationFrame(f); })(0);
  </script></body></html>`);
  const r = await new PlaywrightChecker(300).check(file);
  assert.deepEqual(r.errors, []);
  assert.equal(r.static, false);
  assert.ok(r.screenshot.length > 0);
});

test("reports uncaught errors and a frozen page", opts, async () => {
  const file = await page(`<!doctype html><html><body><p>still</p><script>undefinedThing.go()</script></body></html>`);
  const r = await new PlaywrightChecker(300).check(file);
  assert.match(r.errors.join(), /undefinedThing is not defined/);
  assert.equal(r.static, true);
});
