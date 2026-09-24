# 01 — Game Design Document: Lightbound

## 1. Pitch

You are the last lamplighter, descending into the Hollows: caves where darkness is alive. Thousands of shadow creatures hunt anything outside the light. Light is your only weapon, and every source of it is limited. Push deeper, floor by floor, rationing oil and flares, bending light with mirrors, and turning the swarm's hunger for darkness against it.

- **Genre:** top-down 2.5D survival-stealth roguelite
- **Session:** one run = 5 procedurally generated floors, ~3–6 minutes each
- **Core fantasy:** a single flame against an ocean of darkness; watching a swarm part around your light like water
- **Platforms:** macOS (Apple silicon, Metal) and Windows (Vulkan). Keyboard+mouse and gamepad.

## 2. Design pillars

1. **Light is territory.** Lit areas are safe; the player reshapes the map by where they cast, throw and reflect light.
2. **The swarm reads the light, now.** Creatures respond to the light field *the player sees on screen*, in the same frame. What looks lit *is* lit.
3. **Scarcity creates decisions.** Oil, flares and strobe cooldown are always tight; brightness is a resource.
4. **Emergent, not scripted.** Behavior comes from simple per-creature rules plus alarm propagation, producing flanking, baiting and stampedes.

Pillar 2 is research-critical: creature perception samples the GPU light field every frame (`05_ARCHITECTURE.md`). Under strategy S0 (frame-delayed readback) the pillar visibly breaks; that breakage is a measured outcome, not a bug.

## 3. Core loop

```
Enter floor → explore (find the exit well, loot caches, light braziers)
  → manage swarm pressure (lantern, flares, strobe, positioning)
  → open exit (reach it; on seal floors, route a light beam onto the sun-sigil)
  → descend → shrine: pick 1 of 3 upgrades → next floor (deeper = darker, bigger swarm)
Death → run summary (depth, creatures burned, flares used, oil spent, time)
Victory → clear floor 5 → summary + seed shown (shareable seed)
```

## 4. Player kit

All numbers are starting values in `data/tuning.json` (hot-reloadable in play mode, frozen and hashed into the CSV header in bench mode). World units: 1 tile = 16 px; internal resolution 480×270 (see art doc).

| Tool | Input (KB+M / pad) | Behavior | Starting tuning |
|---|---|---|---|
| Move | WASD / LS | 8-dir with slight acceleration | 70 px/s, accel 600 px/s² |
| Dash | Space / A | Burst, i-frames | 140 px over 0.15 s, CD 1.2 s |
| Lantern | always on, toggle F / Y | Cone light toward cursor / RS | range 110 px, half-angle 35°, intensity 1.0 |
| Lantern focus | hold RMB / LT | Narrow beam; reflects off mirrors | range 170 px, half-angle 12°, intensity 1.8, oil ×3 |
| Oil | resource | Drains while lantern on | max 100, 1.0/s; at 0 → 20 px glow, intensity 0.3 |
| Flare | LMB / RT | Arc throw (max 120 px), burns radially | 6 s, radius 90 px, intensity 1.4, carry 3 (max 5) |
| Strobe | Q / RB | Instant flash, stuns + damages lit creatures | radius 200 px, 0.15 s, intensity 3.0, CD 10 s |
| Ignite | E / X near brazier | Light brazier for rest of floor | cost 15 oil, radius 80 px |
| Rotate | E / X near mirror | Rotate mirror 45° clockwise (hold = counter-clockwise) | — |

**Health:** 5 hearts. Contact with a creature in HUNT/SWARM/FLANK deals 1 heart, then 1.0 s invulnerability and 60 px knockback. A creature whose sampled `lit ≥ flee_threshold` cannot deal damage.

**Light damage** (computed on CPU from perception, pass B):
`dps = k_dmg * max(0, lit - burn_threshold[type])`, `k_dmg = 6 hp/s`.
Strobe adds a flat 1.5 hp to every creature with `lit > 0.1` during the flash frame and sets STUNNED.
hp ≤ 0 → BURNING (0.4 s ember dissolve) → removed.

## 5. Creatures

All creatures share one SoA representation and the same perception record; `type` selects decision parameters.

| Type | Share | hp | Speed px/s | burn_thr | flee_thr | Role |
|---|---|---|---|---|---|---|
| Crawler | 75% | 1.0 | 55 | 0.25 | 0.20 | Swarm body; hunts in dark, flees light gradient |
| Stalker | 15% | 2.0 | 65 | 0.35 | 0.30 | Flanks behind the player, stays out of the lantern cone |
| Wailer | 7% | 1.5 | 45 | 0.25 | 0.15 | When lit, screams: alarm propagation (§5.3) |
| Brute | 3% | 8.0 | 35 | 0.60 | 0.70 | Tolerates light; knocks landed flares away on contact |

Silhouettes must be readable at 8–16 px (see art doc §4).

### 5.1 States

| State | Meaning |
|---|---|
| WANDER | Drift toward nearby darkness, 40% speed, per-agent noise heading |
| HUNT | Move toward player (LOS) or last-known / alarm position |
| SWARM | HUNT + cohesion when `neighbor_count ≥ 6`; +15% speed |
| FLANK | Stalker only: target = player − 60 px × lantern_dir, pathing via dark cells |
| FLEE | Move along −∇lit (toward darker); 120% speed |
| STUNNED | Frozen 1.2 s after strobe |
| BURNING | Dissolving; no movement; then removed |

### 5.2 Decision function (utility scoring)

`decide(AgentState, Perception, WorldCtx) -> Intent` is pure (see CLAUDE.md R3). Each candidate state gets a score; highest wins, with **hysteresis +0.15** for the current state to avoid flicker. Starting formulas:

```
s_flee    = 2.0 * max(0, lit - flee_thr)            + 0.5 * stunned_recent
s_hunt    = 1.0 * los_player * (1 - dist/320)       + 0.8 * alarm
s_swarm   = s_hunt * min(1, neighbor_count / 10)    (only if s_hunt > 0.3)
s_flank   = (type==Stalker) * 0.9 * los_player * in_lantern_front
s_wander  = 0.2
```

Intent = `{ state, desired_dir (float2), desired_speed, target_pos, flags }`.
Per-agent randomness only via `pcg32(floor_seed ^ hash(agent_id), frame_index)` — stateless, reproducible.

### 5.3 Alarm propagation (the irregular CPU part)

- A Wailer with `lit ≥ 0.15` emits alarm strength 1.0 at its position (max once per 2 s per Wailer).
- Propagation is **BFS over the creature neighbor graph** (agents within 48 px), decay ×0.7 per hop, max 4 hops, within the same frame.
- Graph built on CPU from the GPU spatial-hash cell ranges (read in place from the shared perception/hash buffer).
- Alarmed agents get `alarm` in perception for the next decision and target the source.
- With **K ≥ 3**, alarmed agents emit a RefineList entry requesting a GPU LOS ray to the source; confirmed LOS gives +20% speed ("they saw it too").
- Events: each alarm emits an audio event (scream, spatialized) and a VFX ring.

This is deliberately graph-shaped and branchy: exactly the logic engines keep on the CPU.

### 5.4 Ambiguity → refinement (K ≥ 2)

An agent is *ambiguous* if `|lit - flee_thr| < 0.05` or its coarse LOS sample is within 1 tile of an occluder edge. Ambiguous agents are appended to the RefineList; the GPU refines with 8 supersampled light samples and a precise DDA LOS ray. Decisions for those agents are re-run on CPU (pass D). Cap RefineList at 25% of N.

### 5.5 Steering (GPU, pass E)

The GPU integrates Intents: desired velocity + separation (spatial hash, radius 10 px) + wall avoidance (SDF gradient) + smoothing (τ = 0.1 s). The CPU never writes positions.

## 6. Level elements

| Element | Behavior |
|---|---|
| Brazier | Static radial light (80 px); some pre-lit, rest lit with oil |
| Crystal | Weak cold light (40 px); emits a thin beam in a fixed direction on seal floors |
| Mirror | Rotatable 45° steps; reflects focus beams and crystal beams (max 4 bounces) |
| Sun-sigil seal | Exit sealed until a beam hits the sigil continuously for 1.5 s |
| Cache | Oil +40 (50%), flare +1–2 (35%), heart (15%) |
| Nest | Spawns creatures until floor budget met (1 per 0.25 s); destroyed by 6 s focused light |
| Oil pool | Refill station; +60 oil once |
| Exit well | Descend |
| Shrine | Between floors: 1 of 3 upgrades |

## 7. Progression

**Shrine upgrades** (pool; 3 offered, seeded):
Wider Wick (+8° half-angle) · Deep Reservoir (+30 max oil) · Magnesium (flares +3 s) · Echo Flash (strobe CD −3 s) · Mirror Hands (beam +2 bounces) · Ember Heart (+1 heart) · Quick Hands (+1 flare carry) · Slow Burn (oil drain −20%).

**Depth scaling** (floor d = 1..5):

| Param | Formula | d1 → d5 |
|---|---|---|
| Swarm budget | `1500 · 1.6^(d−1)` | 1.5k → ~9.8k |
| Map size (tiles) | `120 + 20(d−1)` square | 120 → 200 |
| Pre-lit brazier fraction | `0.6 − 0.12(d−1)` | 0.6 → 0.12 |
| Ambient light | `0.06 − 0.01(d−1)` | 0.06 → 0.02 |
| Seal floor | d = 3, 5 | — |
| Biome | see procgen doc | Mossy → Crystal → Ember → Abyss |

**Bench mode** overrides swarm size with `--agents` (up to 100k) and disables player death; see research protocol. No meta-progression in v1 (keeps runs comparable).

## 8. Game feel checklist

- Creatures scatter from a flare **on the frame it ignites**.
- Strobe: white flash, chromatic kick, 60 ms hitstop, swarm freezes mid-motion.
- Burn: creatures crackle into orange embers that drift up and fade.
- Low oil (<20): lantern flickers (seeded noise), hum drops in pitch.
- Swarm is heard before seen: chitter density rises with nearby SWARM count.
- Screen shake only on strobe and player hit (small, 4 px max; toggleable).

## 9. UI / HUD

- Oil: thin arc around the player sprite.
- Hearts + flare count: tiny pixel icons, top-left.
- Strobe cooldown: ring at player's feet.
- Floor / depth: top-right, fades after 3 s on floor entry.
- Pause menu: resume, restart with seed, settings (volume, shake, fullscreen), quit.
- **Debug overlay (F3):** fps, frame ms, strategy, K, N, per-pass GPU ms, per-round-trip sync wait µs, RefineList size, staleness (if audit on).
- **Debug views (F4 cycle):** light field, SDF, spatial-hash density, agent states colored, RefineList agents highlighted.

## 10. Out of scope (v1)

Multiplayer, mid-run saves, meta-progression, story/dialogue, rebinding UI, localization, mobile.
