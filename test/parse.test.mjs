import { test } from "node:test";
import assert from "node:assert/strict";
import { extractHtml, extractJson } from "../dist/util/parse.js";

test("extractJson handles fences and surrounding prose", () => {
  assert.deepEqual(extractJson('Here you go:\n```json\n{"a": 1}\n```', "x"), { a: 1 });
  assert.deepEqual(extractJson('{"a": {"b": 2}}', "x"), { a: { b: 2 } });
});

test("extractHtml strips fences and leading prose", () => {
  assert.equal(extractHtml("Sure!\n```html\n<!doctype html><html></html>\n```"), "<!doctype html><html></html>");
  assert.equal(extractHtml("Intro <html><body></body></html>"), "<html><body></body></html>");
});
