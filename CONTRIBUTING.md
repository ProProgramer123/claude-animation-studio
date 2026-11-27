# Contributing

Thanks for helping build Claude Animation Studio.

## Getting set up

```bash
git clone https://github.com/ProProgramer123/claude-animation-studio.git
cd claude-animation-studio
npm install
npx playwright install chromium
npm test
```

`npm test` builds the project, runs the unit tests and starts the MCP server to call the tool. It uses a
mock runner, so it never calls Claude and costs nothing.

To try a real run without Claude Code's UI, build and call the pipeline directly:

```bash
npm run build
node -e '
import("./dist/agents/pipeline.js").then(async ({ runPipeline }) => {
  const { ClaudeCodeRunner } = await import("./dist/runners/claudeCode.js");
  const { createChecker } = await import("./dist/check/playwright.js");
  const r = await runPipeline({
    prompt: "a hot air balloon drifting over hills", mode: "2d",
    runner: new ClaudeCodeRunner(), checker: createChecker(),
    outputDir: "animations/dev-run", onProgress: (_s, m) => console.log(m),
  });
  console.log(r);
});'
```

## Where things live

- **Agent instructions:** `src/agents/prompts.ts`. Most quality improvements start here.
- **The order agents run in, and the fix loop:** `src/agents/pipeline.ts`.
- **How agents reach Claude:** `src/runners/`. To add a backend (for example the Claude API with an API
  key), implement `AgentRunner` from `src/runners/types.ts` and register it in `src/runners/index.ts`.
- **The browser check:** `src/check/`.

## Pull requests

- Keep each PR to one change, and add or update tests for it.
- Make sure `npm run typecheck` and `npm test` pass.
- If you change what the agents produce, include a before/after screenshot or a new folder in
  `examples/` made with a real run.

## Adding an example

Copy a finished folder from `animations/` into `examples/<short-name>/`. Keep every file the agents
wrote, so people can see how the animation was made.
