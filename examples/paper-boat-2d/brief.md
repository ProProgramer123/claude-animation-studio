# Creative Brief: *Paper Boat at Sunset*

**Format:** 2D, single self-contained HTML page (inline SVG + CSS/Web Animations API). No external libraries.
**Length:** 10 seconds, seamless loop.

## Subject
A small white origami paper boat with crisp folded facets, shaded in two tones so the folds read. It is slightly crumpled at the edges, which makes it feel handmade.

## Setting
An open sea at sunset. A low, half-set sun sits on the horizon. The sky moves through a gradient: deep violet at the top, then magenta, then coral, then molten gold at the horizon. The sea holds 3–4 layered wave bands in darker plum and rose tones. A shimmering column of sun reflection stretches toward the viewer. A few thin, flat clouds glow pink on their undersides.

## Mood
Calm, nostalgic, gently hopeful. It should feel like a quiet childhood memory.

## Style
Flat vector illustration with soft gradients. There are no outlines; shapes are defined by color. The palette is warm and limited, and the boat is the only cool-white element, so it pops.

## Key Moments
1. **0–2s:** The scene is already alive. Waves undulate in parallax, with the back layers moving slower. The boat enters from the left edge.
2. **2–7s:** The boat glides steadily left to right, bobbing up and down and rocking ±4° in sync with the front wave. A faint V-shaped wake trails behind it.
3. **~5s:** The boat crosses the sun's reflection. It becomes a silhouette for a moment, and the reflection ripples around it. This is the hero beat.
4. **7–10s:** The boat drifts toward the right edge and exits. Sparkles on the water twinkle. The loop resets without a visible jump.

## Pacing
Slow and unhurried, with no sudden motion. All movement uses sine-based easing. Waves follow a 3–4s cycle.

## Technical Notes
- Scales responsively with the viewBox and fills the viewport.
- Honors `prefers-reduced-motion` by showing a gentle static or minimal-motion frame.
