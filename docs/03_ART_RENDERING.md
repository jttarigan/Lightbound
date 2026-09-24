# 03 — Art Direction & Rendering

## 1. Visual identity

**"Candlelit pixel diorama."** Low-resolution pixel art lit by a modern, smooth 2D lighting model. Darkness is near-total; everything the player sees is defined by light. The swarm reads as a living texture: thousands of small glinting eyes and silhouettes that ripple away from light.

Reference feel (describe, don't copy): classic top-down pixel dungeon crawlers + modern 2D dynamic lighting; warm fire against cold stone; eyes glinting in the dark.

Rules:
- **Darkness is the canvas.** Unlit areas are almost black (ambient 0.02–0.06), never pure black: faint silhouettes stay readable.
- **Warm = safe, cold = uncertain, eyes = danger.** Fire light is warm amber; crystals are cold cyan; creature eyes are the only saturated red-magenta in the scene.
- **Pixel-perfect world, smooth light.** Sprites and tiles render at internal resolution with nearest sampling; lighting is computed at internal resolution but with soft falloff and dithering so it doesn't band.

## 2. Resolution & camera

- Internal resolution: **480×270**, integer-scaled to the window (letterboxed). Fixed for all benchmark conditions (R5).
- Camera: top-down orthographic with a slight "2.5D" treatment: walls are drawn with a 1-tile tall front face (south-facing), sprites are Y-sorted.
- Camera follows player with critical damping (ω = 8), look-ahead 24 px toward aim direction, snapped to whole internal pixels.

## 3. Palette

A 32-color master palette, organized as ramps. All sprites/tiles use it; lighting multiplies colors afterward, so saturation lives in the lights, not the albedo.

| Ramp | Use | Hex (dark → light) |
|---|---|---|
| Stone | walls, floors | `#0b0b10 #16151c #24222b #37333d #4f4953 #6c6470` |
| Moss | biome 1 | `#0f1d17 #1c3326 #2d4d33 #4a6b40` |
| Crystal | biome 2–3, cold lights | `#1a1433 #2e2366 #3f5fb0 #5fc9e0 #b8f4ff` |
| Ember | biome 4, fire | `#2a0f0a #5a1e10 #a8401a #e27a2a #ffc463 #fff1c2` |
| Abyss | biome 5 | `#05070d #0c1426 #1a2a44 #d9d2c0 (bone)` |
| Creature | body / eyes | `#07060a #121019` / eyes `#ff2a6d #ff8fb1` |
| UI | HUD | `#fff1c2 #e27a2a #5fc9e0 #ff2a6d #f2f2f2` |

Light colors: lantern `#ffcf8a` (2700K-ish), flare `#ff8a4a`→`#ffe0a0` (hot core), strobe `#f0f4ff`, brazier `#ff9a3c`, crystal `#6fd8ff`.

## 4. Sprites (procedurally generated at startup — no external art assets required in v1)

Generate into a single texture atlas (1024×1024, RGBA8) at startup from seeded code in `src/render/spritegen/`. Human-made art can replace the atlas later with the same layout.

| Sprite | Size | Frames | Notes |
|---|---|---|---|
| Player (lamplighter) | 16×16 | idle 4, walk 6×4 dirs, dash 3 | hooded figure, lantern held on aim side |
| Crawler | 8×8 | move 4 | low, many-legged blob, 2 eye pixels |
| Stalker | 10×12 | move 4 | tall, thin, hunched, 2 eyes high |
| Wailer | 10×10 | move 4, scream 3 | round, big mouth; eyes widen on scream |
| Brute | 16×16 | move 4, hit 2 | broad shoulders, 3 eyes |
| Tiles | 16×16 | autotile 47-blob set per biome | wall top, wall face, floor variants ×4 |
| Props | 16×16 / 16×24 | brazier (unlit, lit 4f), crystal, mirror (8 rotations), cache, nest (pulse 4f), oil pool, well, sigil | |
| Particles | 1–4 px | — | embers, sparks, smoke puffs, dust |

Creature generator: a symmetric noise mask on a small grid (like space-invader generators) with fixed body silhouette per type + per-variant jitter (8 variants/type), dark body color, bright eye pixels on an **emissive layer** (eyes glow even in darkness at intensity 0.8 — this is how the player "sees" the swarm).

Animation: 8–10 fps flipbook. Swarm agents get a random phase offset (from agent_id) so crowds don't animate in lockstep.

## 5. Render pipeline (per frame, all at 480×270 unless stated)

Passes are GPU passes in the frame pipeline (`05_ARCHITECTURE.md`); the research passes A/C also produce the light field, so lighting is shared between gameplay and visuals (pillar 2).

1. **Light field (pass A, compute)** — see §6. Output: `LightTex` RGBA16F 480×270 (rgb = light color×intensity, a = luminance used for gameplay) plus a 2× downsampled luminance mip for gradient sampling.
2. **Albedo pass (raster)** — tiles (instanced quads from a tile buffer), props, player, creatures (**one instanced draw for all agents**, instance data read directly from the agent position/state buffers written by pass E), deco. Y-sorted by instance key (walls/props/player sorted on CPU, agents sorted by a GPU bitonic/radix key sort on y). Outputs `AlbedoTex` RGBA8 and `EmissiveTex` RGBA8 (eyes, embers, lit brazier flames, crystal cores).
3. **Composite (compute or fullscreen)** — `color = albedo * (ambient + LightTex.rgb) + emissive`; normal-less, but walls' south faces get +15% light for 2.5D readability.
4. **Particles** — additive GPU particles (embers, sparks, smoke) with simple Euler integration; cap 32k particles; spawned from gameplay events via an event buffer.
5. **Post (compute, at internal res)**
   - Bloom: 4-mip dual-Kawase on emissive + bright light (threshold 1.0), strength 0.35.
   - Vignette: 0.25; stronger (0.5) when low oil.
   - Film grain: 1-bit blue-noise dither, amplitude 1/255 × 2, animated per frame — kills banding in dark gradients.
   - Chromatic kick: on strobe only, 2 px for 80 ms.
   - Color grading: per-biome 16³ LUT.
6. **UI** — pixel UI drawn at internal resolution (bitmap font 5×7, generated at startup), before upscale, so the look stays consistent. The F3 debug overlay is the exception: drawn after upscale at native resolution for legibility.
7. **Upscale** — integer nearest-neighbour to the swapchain, letterboxed.

Fixed caps (R5): max 64 static lights + 16 dynamic lights (lantern, flares, strobe, beams), 32k particles, bloom 4 mips. Identical in every condition.

## 6. Lighting model (the research-relevant part)

2D lighting with hard-edged occlusion softened by the SDF:

- **Occlusion:** per light, per pixel: march the SDF from pixel to light (sphere tracing, max 24 steps, early-out) to compute visibility with a soft penumbra: `vis = clamp(min_i( k * sdf_i / t_i ), 0, 1)`, `k = 8`. Walls themselves are lit on their face only if the light is on the facing side.
- **Falloff:** `I(r) = intensity * (1 - (r/R)^2)^2` (smooth, zero at R).
- **Cone lights (lantern):** angular falloff `smoothstep(cos(half), cos(half*0.8), dot(dir, L))`.
- **Beams (focus + crystal beams):** traced on the GPU as up to 5 segments (mirror reflections) and rendered as capsule lights of width 6 px along each segment. Mirror hit detection uses the prop buffer. Beam segment endpoints are also written to a small buffer the CPU reads for the seal puzzle (same-frame).
- **Light culling:** tile-based (16×16 px tiles, 30×17 grid); each tile gets a list of up to 16 lights overlapping it. Static lights' tile lists are built once per floor; dynamic lights appended per frame.
- **Gameplay sampling:** the perception kernel (pass A second half) samples `LightTex.a` (luminance) at each agent position and its central-difference gradient from the half-res mip. **Visual light = gameplay light**, the same texture.
- **Off-screen agents:** light is only computed in a view-padded region (camera view + 64 px border). Agents outside it use a coarse **world light grid** (8 px cells, whole floor, updated at 15 Hz from static lights + dynamic lights without occlusion softness, with SDF-based hard occlusion). Both are in pass A so the workload is fixed.

## 7. Readability & accessibility

- Creature eyes must remain visible in total darkness (emissive).
- Optional high-contrast mode: raise ambient to 0.12 and add 1 px dark outline to creatures.
- Screen shake / chromatic / flash intensity sliders (flash reduction halves strobe brightness **visually only**; gameplay lighting unchanged).
- Colorblind: state signals never rely only on red vs green; alarms add a ring shape.

## 8. Performance budget (targets, Release, 20k agents, M5 and 3060 Ti)

| Pass | Target |
|---|---|
| Light field + perception (A) | ≤ 2.0 ms |
| Albedo + sort | ≤ 1.5 ms |
| Composite + particles + post | ≤ 1.0 ms |
| Steering/integration (E) | ≤ 0.8 ms |

These are guides for the game, not research outcomes; don't optimize one backend alone (R1).
