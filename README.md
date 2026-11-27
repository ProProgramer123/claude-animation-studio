# Claude Animation Studio

An AI animation studio that runs inside Claude Code. You describe an animation, a hidden
director refines your prompt into a detailed brief, and five agents build it together in
2D or 3D.

> **Status:** early development. The MCP server and the `create_animation` tool are in place;
> the agent pipeline and real 2D/3D output are coming next. See the [roadmap](#roadmap).

## How it works

```
your prompt ──► Director (hidden) ──► Storyboard ──► Designer ──► Animator ──► Renderer ──► Reviewer
                refines the brief      scenes &       characters,   movement &   working      checks and
                                       timing         colors        keyframes    code         sends fixes back
```

| Mode | Output |
| ---- | ------ |
| `2d` | An animated SVG/canvas page you can open in a browser |
| `3d` | A Three.js scene |

## Install

Requires Node.js 18 or newer.

```bash
git clone https://github.com/ProProgramer123/claude-animation-studio.git
cd claude-animation-studio
npm install
npm run build
```

## Connect it to Claude Code

From the repo folder:

```bash
claude mcp add animation-studio -- node "$(pwd)/dist/index.js"
```

Then start Claude Code and ask for an animation, for example:

> Use the animation studio to make a 3D animation of a paper plane flying through clouds.

Claude Code calls the `create_animation` tool for you.

## The `create_animation` tool

| Argument | Type | Default | Description |
| -------- | ---- | ------- | ----------- |
| `prompt` | string | required | What the animation should show |
| `mode` | `"2d"` or `"3d"` | `"2d"` | Which kind of animation to make |

## Development

```bash
npm run dev        # run the server from source with tsx
npm run typecheck  # type-check without building
npm test           # build, start the server and call the tool over MCP
```

## Roadmap

1. ~~Project setup and MCP server~~
2. Hidden director and the five agents
3. Real 2D (SVG/canvas) and 3D (Three.js) output, with MP4 export later
4. Example animations, setup docs and a demo GIF

## License

[MIT](LICENSE)
