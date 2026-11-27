/** Pull the first JSON object out of a model reply, tolerating ```json fences and stray prose. */
export function extractJson<T = unknown>(text: string, agent: string): T {
  const fenced = text.match(/```(?:json)?\s*([\s\S]*?)```/i);
  const candidate = fenced ? fenced[1] : text;
  const start = candidate.indexOf("{");
  const end = candidate.lastIndexOf("}");
  if (start === -1 || end <= start) {
    throw new Error(`${agent} agent did not return JSON.`);
  }
  try {
    return JSON.parse(candidate.slice(start, end + 1)) as T;
  } catch (err) {
    throw new Error(`${agent} agent returned invalid JSON: ${(err as Error).message}`);
  }
}

/** Pull an HTML document out of a model reply, with or without a ```html fence. */
export function extractHtml(text: string): string {
  const fenced = text.match(/```html\s*([\s\S]*?)```/i);
  const html = (fenced ? fenced[1] : text).trim();
  const start = html.search(/<!doctype html|<html/i);
  return start > 0 ? html.slice(start) : html;
}
