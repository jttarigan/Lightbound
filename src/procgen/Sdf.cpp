// Stage 3 (docs/02 §3): distance field. Exact tile-level EDT (Felzenszwalb & Huttenlocher) with
// the nearest feature tile, carried to 4 samples per tile, refined by a jump-flood pass over sample
// offsets 4, 2, 1. A sample's value is the exact distance from the sample center to the square of
// its candidate tile (integer squared distance, one sqrt), so the result is bit-identical on every
// IEEE platform. Open samples: + distance to the nearest wall square. Wall samples: − distance to
// the nearest open square.
#include "procgen/Internal.h"

#include <cmath>

namespace lb::procgen::detail {
namespace {

constexpr f64 kInf = 1e20;

/// 1-D squared distance transform of f (length n). d[q] = min_p (q − p)² + f[p], arg[q] = argmin p.
void edt1d(const f64* f, i32 n, f64* d, i32* arg, i32* v, f64* z) {
    i32 k = 0;
    v[0] = 0;
    z[0] = -kInf;
    z[1] = kInf;
    for (i32 q = 1; q < n; ++q) {
        f64 s = 0.0;
        for (;;) {  // z[0] = -inf, so k never drops below 0
            const i32 vk = v[k];
            s = ((f[q] + static_cast<f64>(q) * q) - (f[vk] + static_cast<f64>(vk) * vk)) / (2.0 * q - 2.0 * vk);
            if (s > z[k]) break;
            --k;
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = kInf;
    }
    k = 0;
    for (i32 q = 0; q < n; ++q) {
        while (z[k + 1] < q) ++k;
        const f64 dq = static_cast<f64>(q - v[k]);
        d[q] = dq * dq + f[v[k]];
        arg[q] = v[k];
    }
}

/// Nearest feature tile (index, or -1) for every tile.
void nearestFeature(const Gen& g, bool wantWall, std::vector<i32>& nearest) {
    const i32 w = g.w, h = g.h, n = w > h ? w : h;
    std::vector<f64> colD(static_cast<usize>(w * h)), f(static_cast<usize>(n)), d(static_cast<usize>(n));
    std::vector<f64> z(static_cast<usize>(n + 1));
    std::vector<i32> colArg(static_cast<usize>(w * h)), arg(static_cast<usize>(n)), v(static_cast<usize>(n));
    for (i32 x = 0; x < w; ++x) {
        for (i32 y = 0; y < h; ++y) f[static_cast<usize>(y)] = (blocksLight(g.at(x, y)) == wantWall) ? 0.0 : kInf;
        edt1d(f.data(), h, d.data(), arg.data(), v.data(), z.data());
        for (i32 y = 0; y < h; ++y) {
            colD[static_cast<usize>(y * w + x)] = d[static_cast<usize>(y)];
            colArg[static_cast<usize>(y * w + x)] = arg[static_cast<usize>(y)];
        }
    }
    nearest.assign(static_cast<usize>(w * h), -1);
    for (i32 y = 0; y < h; ++y) {
        for (i32 x = 0; x < w; ++x) f[static_cast<usize>(x)] = colD[static_cast<usize>(y * w + x)];
        edt1d(f.data(), w, d.data(), arg.data(), v.data(), z.data());
        for (i32 x = 0; x < w; ++x) {
            if (d[static_cast<usize>(x)] >= kInf * 0.5) continue;
            const i32 ax = arg[static_cast<usize>(x)];
            nearest[static_cast<usize>(y * w + x)] = colArg[static_cast<usize>(y * w + ax)] * w + ax;
        }
    }
}

} // namespace

void stageDistanceField(Gen& g) {
    FloorData& f = g.f;
    const i32 w = g.w, h = g.h;
    std::vector<i32> nearWall, nearOpen;
    nearestFeature(g, true, nearWall);
    nearestFeature(g, false, nearOpen);

    // Candidates are packed as (y << 16) | x so an evaluation needs no division.
    auto pack = [w](i32 tile) -> i32 { return tile < 0 ? -1 : ((tile / w) << 16) | (tile % w); };
    for (i32& c : nearWall) c = pack(c);
    for (i32& c : nearOpen) c = pack(c);
    auto eval = [](i32 px, i32 py, i32 c) -> i64 {
        return c < 0 ? INT64_MAX : pointToTileD2(px, py, c & 0xFFFF, c >> 16);
    };

    // Tile centers of open tiles (used for brazier, cache and nest placement).
    g.tileD2.assign(static_cast<usize>(w * h), 0);
    for (i32 y = 0; y < h; ++y)
        for (i32 x = 0; x < w; ++x) {
            if (blocksLight(g.at(x, y))) continue;
            const i32 px = x * kTilePx + kTilePx / 2, py = y * kTilePx + kTilePx / 2;
            i64 best = INT64_MAX;
            for (i32 oy = -1; oy <= 1; ++oy)
                for (i32 ox = -1; ox <= 1; ++ox) {
                    const i32 nx = x + ox, ny = y + oy;
                    if (!g.inside(nx, ny)) continue;
                    const i64 d2 = eval(px, py, nearWall[static_cast<usize>(ny * w + nx)]);
                    if (d2 < best) best = d2;
                }
            g.tileD2[static_cast<usize>(y * w + x)] = best;
        }

    // Samples: initial candidate from the containing tile, then jump flood over offsets 4, 2, 1.
    const i32 sw = w * kSdfPerTile, sh = h * kSdfPerTile;
    std::vector<i32> cur(static_cast<usize>(sw * sh)), nxt(cur.size());
    std::vector<u8> boundary(static_cast<usize>(w * h), 0);
    for (i32 y = 0; y < h; ++y)
        for (i32 x = 0; x < w; ++x)
            for (i32 k = 0; k < 8; ++k)
                if (g.inside(x + kDx8[k], y + kDy8[k]) && isOpen(g.at(x, y)) != isOpen(g.at(x + kDx8[k], y + kDy8[k]))) {
                    boundary[static_cast<usize>(y * w + x)] = 1;
                    break;
                }
    std::vector<u8> openS(cur.size()), refine(cur.size());
    constexpr i64 kDeepWallD2 = 3 * 3;  // tiles²: wall tiles farther from open space keep the EDT candidate
    for (i32 sy = 0; sy < sh; ++sy)
        for (i32 sx = 0; sx < sw; ++sx) {
            const usize s = static_cast<usize>(sy * sw + sx);
            const i32 tx = sx / kSdfPerTile, ty = sy / kSdfPerTile;
            const usize t = static_cast<usize>(ty * w + tx);
            openS[s] = isOpen(f.tiles[t]) ? 1 : 0;
            const std::vector<i32>& near = openS[s] != 0 ? nearWall : nearOpen;
            i32 best = near[t];
            if (boundary[t] != 0) {  // tile touches the other class: also try those tiles and the neighbors' features
                const i32 px = sx * kSdfSamplePx + kSdfSamplePx / 2, py = sy * kSdfSamplePx + kSdfSamplePx / 2;
                i64 bestD2 = eval(px, py, best);
                for (i32 k = 0; k < 8; ++k) {
                    const i32 nx = tx + kDx8[k], ny = ty + kDy8[k];
                    if (!g.inside(nx, ny)) continue;
                    const usize nt = static_cast<usize>(ny * w + nx);
                    const bool sameClass = isOpen(f.tiles[nt]) == (openS[s] != 0);
                    const i32 c = sameClass ? near[nt] : ((ny << 16) | nx);
                    const i64 d2 = eval(px, py, c);
                    if (d2 < bestD2 || (d2 == bestD2 && c >= 0 && c < best)) {
                        best = c;
                        bestD2 = d2;
                    }
                }
            }
            cur[s] = best;
            const i32 c = cur[s];
            refine[s] = (openS[s] != 0 || (c >= 0 && sq((c & 0xFFFF) - tx) + sq((c >> 16) - ty) <= kDeepWallD2)) ? 1 : 0;
        }
    nxt = cur;
    for (i32 step : {4, 2, 1}) {
        for (i32 sy = 0; sy < sh; ++sy)
            for (i32 sx = 0; sx < sw; ++sx) {
                const usize s = static_cast<usize>(sy * sw + sx);
                if (refine[s] == 0) continue;
                const i32 px = sx * kSdfSamplePx + kSdfSamplePx / 2, py = sy * kSdfSamplePx + kSdfSamplePx / 2;
                i32 best = cur[s];
                i64 bestD2 = eval(px, py, best);
                for (i32 k = 0; k < 8; ++k) {
                    const i32 nx = sx + kDx8[k] * step, ny = sy + kDy8[k] * step;
                    if (nx < 0 || ny < 0 || nx >= sw || ny >= sh) continue;
                    const usize ns = static_cast<usize>(ny * sw + nx);
                    if (openS[ns] != openS[s]) continue;
                    const i32 c = cur[ns];
                    if (c == best) continue;
                    const i64 d2 = eval(px, py, c);
                    if (d2 < bestD2 || (d2 == bestD2 && c >= 0 && c < best)) {
                        best = c;
                        bestD2 = d2;
                    }
                }
                nxt[s] = best;
            }
        cur.swap(nxt);
    }

    f.sdf.resize(cur.size());
    for (i32 sy = 0; sy < sh; ++sy)
        for (i32 sx = 0; sx < sw; ++sx) {
            const usize s = static_cast<usize>(sy * sw + sx);
            const i32 px = sx * kSdfSamplePx + kSdfSamplePx / 2, py = sy * kSdfSamplePx + kSdfSamplePx / 2;
            const i64 d2 = eval(px, py, cur[s]);
            const f32 dist = d2 == INT64_MAX ? 1e6f : static_cast<f32>(std::sqrt(static_cast<f64>(d2)));
            f.sdf[s] = openS[s] != 0 ? dist : -dist;
        }
}

} // namespace lb::procgen::detail
