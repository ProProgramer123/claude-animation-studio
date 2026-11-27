# Claude Animation Studio

An AI animation studio that runs inside Claude Code. You describe an animation, a hidden
director refines your prompt into a detailed brief, and five agents build it together in
2D or 3D.

<p align="center">
  <img src="docs/paper-boat-2d.gif" alt="A paper boat sailing across a sunset sea, made in 2D mode" width="49%">
  <img src="docs/lighthouse-3d.gif" alt="A lighthouse at night with its beam sweeping over the waves, made in 3D mode" width="49%">
</p>
<p align="center"><sub>
  Left: <i>"a paper boat sailing across a sunset sea"</i> (2D). Right: <i>"a lighthouse on a rocky island at night, its beam sweeping over the waves"</i> (3D).<br>
  Each was made from that one sentence. Open them yourself in <a href="examples"><code>examples/</code></a>.
</sub></p>

> **Status:** early, but working end to end in both modes. See the [roadmap](#roadmap).

## Quick start

You need [Node.js](https://nodejs.org) 18+ and [Claude Code](https://code.claude.com), signed in.

```bash
git clone https://github.com/ProProgramer123/claude-animation-studio.git
cd claude-animation-studio
npm install && npm run build
npx playwright install chromium   # optional: lets the studio test each animation in a browser
claude mcp add --scope user animation-studio -- node "$(pwd)/dist/index.js"
```

Then start Claude Code in any folder and ask:

> Use the animation studio to make a 3D animation of a paper plane flying through clouds.

A few minutes later you'll get a link to an HTML file. Open it in your browser.

## Native C++ app (in progress)

The studio is being turned into a native desktop app written in C++, so you can animate
without Claude Code's chat or Node.js. The engine is done: the same hidden director and five
agents, ported to C++17 in [`cpp/`](cpp), with a command-line app you can use today. A desktop
window with live progress and a built-in preview comes next.

You need a C++17 compiler, CMake 3.16+ and [Claude Code](https://code.claude.com), signed in.

```bash
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build --config Release
./cpp/build/animation-studio "a paper boat sailing across a sunset sea"
./cpp/build/animation-studio --mode 3d "a lighthouse at night"
./cpp/build/animation-studio --runner mock "anything"   # offline, with canned agents
```

Run `ctest --test-dir cpp/build` for the engine tests. Agents still run through your Claude Code
login, so no API key is needed.

## How it works

```
your prompt ──► Director (hidden) ──► Storyboard ──► Designer ──► Animator ──► Renderer ──► Reviewer
                refines the brief      scenes &       characters,   movement &   working      checks and
                                       timing         colors        keyframes    code         sends fixes back
```

The director's brief and every agent's output are saved next to the finished animation, so you can
see how it was made:

```
animations/2026-11-27T21-40-00-a-paper-boat/
  prompt.txt        what you asked for
  brief.md          the director's refined brief
  storyboard.json   scenes and timing
  design.json       characters, colors and objects
  animation.json    keyframes
  index.html        the finished animation (open it in a browser)
  preview.png       a screenshot from the browser check
  review.json       the reviewer's notes and how many fix rounds it took
```

| Mode | Output |
| ---- | ------ |
| `2d` | An animated SVG/canvas page you can open in a browser |
| `3d` | A Three.js scene (loads Three.js from the jsDelivr CDN, so it needs an internet connection) |

## Setup in detail

### 1. Install

The agents run on your own Claude Code login, so no API key is needed.

```bash
git clone https://github.com/ProProgramer123/claude-animation-studio.git
cd claude-animation-studio
npm install
npm run build
```

### 2. Connect it to Claude Code

From the repo folder:

```bash
claude mcp add --scope user animation-studio -- node "$(pwd)/dist/index.js"
```

`--scope user` makes the studio available in every folder. Leave it out to enable it only for the folder
you run the command in. Check it's connected with `claude mcp list`.

### 3. Make an animation

Start Claude Code and ask for an animation in plain words. Say "2D" or "3D" if you have a preference;
2D is the default. Claude Code calls the `create_animation` tool for you.

A run takes a few minutes, since six agents work one after another. Animations are saved in an
`animations/` folder inside the folder you started Claude Code from.

### Browser check (recommended)

Before the reviewer looks at an animation, the studio can open it in a headless browser. Any errors, and
a page where nothing moves, are passed to the reviewer and sent back to the renderer to fix. It also saves
a `preview.png`. This needs Playwright's Chromium, installed once:

```bash
npx playwright install chromium
```

Without it the studio still works and skips this check.

## Settings

| Environment variable | Default | What it does |
| -------------------- | ------- | ------------ |
| `ANIMATION_STUDIO_OUTPUT_DIR` | `animations` | Where finished animations are saved |
| `ANIMATION_STUDIO_MODEL` | your Claude Code default | Model the agents use, for example `sonnet` or `opus` |
| `ANIMATION_STUDIO_RUNNER` | `claude-code` | `claude-code` runs real agents; `mock` returns canned replies for testing |
| `ANIMATION_STUDIO_BROWSER_CHECK` | `on` | `off` skips the headless browser check |
| `ANIMATION_STUDIO_CHROMIUM_PATH` | Playwright's Chromium | Use a different Chrome or Chromium binary for the check |
| `CLAUDE_CODE_PATH` | `claude` | Path to the Claude Code CLI if it isn't on your `PATH` |

Pass them with `claude mcp add`, for example:

```bash
claude mcp add --scope user animation-studio -e ANIMATION_STUDIO_MODEL=sonnet -- node "$(pwd)/dist/index.js"
```

## The `create_animation` tool

| Argument | Type | Default | Description |
| -------- | ---- | ------- | ----------- |
| `prompt` | string | required | What the animation should show |
| `mode` | `"2d"` or `"3d"` | `"2d"` | Which kind of animation to make |

## Troubleshooting

| Problem | Fix |
| ------- | --- |
| Claude Code stops waiting before the animation is done | Start it with a longer tool timeout: `MCP_TOOL_TIMEOUT=900000 claude` (15 minutes) |
| "Could not start Claude Code" | Make sure `claude` works in your terminal, or set `CLAUDE_CODE_PATH` to its full path |
| An agent fails with a sign-in or usage-limit error | Run `claude` on its own and sign in, or wait for your usage limit to reset |
| "Skipping the browser check" in the logs | Run `npx playwright install chromium` in the repo folder, or ignore it: the studio still works |
| A 3D animation shows a blank page | 3D pages load Three.js from jsDelivr, so check your internet connection |
| The tool doesn't show up in Claude Code | Run `claude mcp list`. If it's missing, run the `claude mcp add` command again from the repo folder |

## Development

```bash
npm run dev        # run the server from source with tsx
npm run typecheck  # type-check without building
npm test           # build, run unit tests, then call the tool over MCP with the mock runner
```

### Project layout

```
src/
  index.ts              starts the MCP server on stdio
  server.ts             registers the tools
  tools/createAnimation.ts
  agents/roster.ts      the director and the five agents
  agents/prompts.ts     each agent's instructions, plus 2D/3D guidance
  agents/pipeline.ts    runs the agents in order, with the reviewer's fix loop
  check/                headless browser check (optional Playwright)
  runners/              how agents reach Claude (Claude Code CLI today; add an API-key runner here)
```

## Roadmap

1. ~~Project setup and MCP server~~
2. ~~Hidden director and the five agents~~
3. ~~Real 2D (SVG/canvas) and 3D (Three.js) output, checked in a headless browser~~ (MP4 export later)
4. ~~Example animations, setup docs and demo GIFs~~
5. Native C++ app: ~~engine and command line~~, then a Qt desktop window, then one-click GIF/MP4 export and installers
6. Ideas: MP4/GIF export, an API-key runner, more styles (pixel art, motion graphics), longer multi-scene films

## Contributing

Ideas, bug reports and pull requests are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE)
