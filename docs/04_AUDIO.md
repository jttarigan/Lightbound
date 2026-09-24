# 04 — Audio Direction

## 1. Identity

**"The cave breathes; the swarm whispers; fire answers."** Sparse, close, textural. Silence and low drones dominate; the swarm is a granular hiss that swells as danger grows; the player's fire is the only warm, tonal sound. Music is minimal and adaptive: it grows out of the ambience rather than sitting on top of it.

All audio is **synthesized procedurally at startup** (`src/audio/synth/`) into PCM buffers — no asset files in v1. Files may replace generated buffers later using the same IDs.

- Library: miniaudio (low-level device API + own mixer). 48 kHz, stereo, float32, 256-frame buffer.
- Audio thread is separate from game and render; communicates via a lock-free SPSC event queue. **Audio must never block the frame** or affect simulation (determinism R4).
- In bench mode audio runs with a null device by default (`--audio=off|null|on`); the mixer still consumes events so CPU cost is comparable when `null`.

## 2. Mix structure

```
Master (limiter -1 dBFS)
 ├─ Ambience bus   (-18 dB)   biome drone + drips + wind + distant rumbles
 ├─ Swarm bus      (-12 dB)   granular chitter clouds (see §4)
 ├─ SFX bus        (-6 dB)    player, tools, creatures, props
 ├─ Music bus      (-14 dB)   adaptive layers (§5)
 └─ UI bus         (-10 dB)
Reverb send: shared cave reverb (Schroeder/FDN, RT60 by biome: 1.8–3.5 s)
Low-pass "darkness filter" on SFX+Swarm: cutoff follows player's local light (dark = muffled, 1.8 kHz → 12 kHz)
```

Spatialization: stereo pan by x offset from camera + distance attenuation (inverse, ref 32 px, max 320 px) + occlusion low-pass when SDF-march from listener to source hits a wall (reuse the CPU copy of the tile grid; cheap 2D DDA).

## 3. Sound list (procedural recipes)

| ID | Sound | Recipe (starting point) |
|---|---|---|
| `lantern_hum` | Lantern loop | pink noise → bandpass 400 Hz Q2 + sine 110 Hz at -24 dB; pitch & gain follow oil (low oil: −3 semitones, flicker AM) |
| `focus_on/off` | Focus beam | rising/falling filtered noise sweep 200 ms + glassy sine 880 Hz |
| `flare_throw` | Throw | short whoosh: white noise, bandpass sweep 2k→600 Hz, 180 ms |
| `flare_ignite` | Ignite | noise burst + crackle impulses (Poisson 60/s) decaying over 400 ms, then `flare_burn` loop |
| `flare_burn` | Burning loop | brown noise LP 1.2 kHz + crackle (Poisson 25/s), fade last 1 s |
| `strobe` | Strobe | 30 ms white-noise click + 2 s shimmering FM tail (carrier 1.2 kHz, ratio 3.5), heavy reverb send; 60 ms hitstop duck of all other buses −12 dB |
| `dash` | Dash | cloth whoosh: noise BP 1.5 kHz, 120 ms |
| `hurt` | Player hit | low thump (sine 70 Hz pitch-drop) + dissonant stab |
| `step_*` | Footsteps | per-biome: stone tick, wet splash (water tiles), crystal tink |
| `burn_die` | Creature burn | crackle burst + tiny high sizzle; pitch varies by type (Crawler high, Brute low) |
| `wailer_scream` | Alarm | 3 detuned saw oscillators glide 900→400 Hz, formant filter "ah", 700 ms, strong reverb |
| `brute_step` | Brute | sub thump 45 Hz every step |
| `brazier_ignite` | Ignite | whoomp (LP noise swell) + crackle loop at brazier (spatial) |
| `mirror_rotate` | Rotate | stone grind (noise through comb filter) 250 ms + click |
| `beam_hit_sigil` | Sigil charging | rising choir-like pad (additive, 5 partials) over 1.5 s; on complete: bell (FM, 2.4 kHz, long decay) |
| `cache_open` | Loot | wooden creak + coin-like tinkles |
| `descend` | Floor exit | downward pitch sweep of the ambience + low boom |
| UI | Menu | soft ticks and blips from the UI palette |

Every one-shot gets ±3% pitch and ±1.5 dB gain variation from a seeded audio RNG (separate from sim RNG; R4).

## 4. Swarm audio (the signature)

Thousands of creatures cannot each own a voice. Use **density-driven granular clouds**:

- Once per sim tick, the CPU (pass B, cheap) bins creatures around the listener into 8 angular sectors × 3 distance rings (24 cells), counting agents per state group: *calm* (WANDER), *hunting* (HUNT/FLANK/SWARM), *fleeing* (FLEE), *stunned*.
- Each cell drives a granular voice: grain source = short procedurally generated chitter samples (8 variants: clicky noise bursts through resonant filters 2–5 kHz), grain rate ∝ `sqrt(count)`, gain ∝ `log(1+count)`, pan = sector angle, LP cutoff by ring.
- **Hunting** grains are faster and brighter; **fleeing** grains are skittering and pitch up; **stunned** cells go silent (the "freeze" after a strobe is heard as a sudden hush).
- Max 24 swarm voices total, fixed cost regardless of N (keeps audio CPU flat across the experimental sweep).

## 5. Adaptive music

Minimal drone-based score in D Phrygian-ish mode (dark, modal). Four layers, all synthesized, crossfaded by a **tension** value `T ∈ [0,1]`:

```
T = clamp( 0.5*hunting_near/200 + 0.3*(1 - oil/100) + 0.2*(hearts_lost/5), 0, 1 )   smoothed τ = 2 s
```

| Layer | Active when | Content |
|---|---|---|
| L0 Drone | always | detuned sine/saw pad on D1 + A1, slow filter LFO |
| L1 Pulse | T > 0.25 | low heartbeat (sine 55 Hz thumps) at 60→100 bpm with T |
| L2 Strings | T > 0.5 | bowed-like texture: filtered saw clusters, minor 2nd dissonances |
| L3 Percussion | T > 0.75 | metallic hits (FM) and tom-like bursts, irregular 7/8 |

Stingers: strobe (reverse cymbal swell into hit), sigil complete (bell + major chord, the only major chord in the game), floor descend (drone drops a 4th), death (all layers cut to a single low drone, 3 s).

Per biome: drone root and timbre change (Mossy: warm pad; Crystal: glassy bells; Ember: distorted low saw; Abyss: sub + wind only).

## 6. Implementation notes

- Voice pool: 64 SFX voices, oldest/quietest stealing.
- Event struct from sim: `{ id, pos, gain, pitch, flags }`, produced by pass B and D (gameplay events), pushed after the frame's CPU passes complete.
- Generated buffer cache: all procedural sounds rendered at startup (< 300 ms total target) with fixed seeds; hash logged.
- Settings: master/music/SFX/ambience volume, mono toggle.
