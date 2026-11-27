# Creative Brief: "The Keeper's Light"

**Format:** 3D, single self-contained HTML page rendering a Three.js scene (Three.js 0.170.0 via import map from jsDelivr, `import * as THREE from "three"`). Seamless loop.

**Total length:** 10 seconds (one full beam rotation per loop).

## Subject
A classic white-and-red striped lighthouse with a glass lantern room and a glowing lamp. A rotating volumetric beam (a soft, additive-blended cone) sweeps a full 360°.

## Setting
- **Island:** A small cluster of jagged, low-poly dark granite rocks. A tiny keeper's cottage with one warm lit window.
- **Sea:** An animated ocean plane with vertex-displaced rolling waves in deep navy. Where the beam passes, it picks up specular glints.
- **Sky:** Near-black indigo gradient, a scatter of faint stars, and a pale crescent moon low on the horizon.
- **Atmosphere:** Light exponential fog, so the beam fades into distance.

## Mood
Solitary, calm, quietly watchful. Peaceful, with no storm.

## Style
Stylized realism with a low-poly, faceted look. Flat-shaded rocks, with smooth water and beam. The palette is cool blues and blacks, broken by warm amber-gold from the lamp and window.

## Key Moments
1. **0–2s:** Wide establishing shot. The camera drifts slowly toward the island and the beam swings toward the viewer.
2. **2–4s:** The beam passes near the camera, giving a soft bloom flash across the frame.
3. **4–8s:** The beam rakes across the waves and lights up the moving crests. The camera continues a gentle orbit.
4. **8–10s:** The beam completes its rotation and the scene returns to its opening state for the loop.

## Pacing
Slow and steady. The beam rotates at a constant speed, and the waves roll in a gentle swell. The camera moves subtly and never cuts.

## Technical Notes
- A point light at the lamp, and a spotlight that rotates with the beam mesh.
- Responsive canvas with a resize handler, and no external assets.
