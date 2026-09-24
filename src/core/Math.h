#pragma once
// Small vector math. float2/float4/uint2/uint4 carry the alignment that std430 gives
// them so C++ structs containing them lay out exactly like the Slang structs
// (see shaders/shared/layouts.slang). float3 is NOT GPU-visible; never put it in a
// GPU struct.
#include "core/Types.h"

#include <cmath>

namespace lb {

struct alignas(8) float2 {
    f32 x = 0.f, y = 0.f;
    constexpr float2() = default;
    constexpr float2(f32 x_, f32 y_) : x(x_), y(y_) {}
    constexpr float2 operator+(float2 o) const { return {x + o.x, y + o.y}; }
    constexpr float2 operator-(float2 o) const { return {x - o.x, y - o.y}; }
    constexpr float2 operator*(f32 s) const { return {x * s, y * s}; }
    constexpr float2 operator/(f32 s) const { return {x / s, y / s}; }
    constexpr float2 operator-() const { return {-x, -y}; }
    constexpr float2& operator+=(float2 o) { x += o.x; y += o.y; return *this; }
    constexpr float2& operator-=(float2 o) { x -= o.x; y -= o.y; return *this; }
    constexpr float2& operator*=(f32 s) { x *= s; y *= s; return *this; }
    constexpr bool operator==(float2 o) const { return x == o.x && y == o.y; }
};
static_assert(sizeof(float2) == 8 && alignof(float2) == 8);

struct float3 {
    f32 x = 0.f, y = 0.f, z = 0.f;
    constexpr float3() = default;
    constexpr float3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}
    constexpr float3 operator+(float3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr float3 operator-(float3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr float3 operator*(f32 s) const { return {x * s, y * s, z * s}; }
    constexpr float3 operator*(float3 o) const { return {x * o.x, y * o.y, z * o.z}; }
};
static_assert(sizeof(float3) == 12);

struct alignas(16) float4 {
    f32 x = 0.f, y = 0.f, z = 0.f, w = 0.f;
    constexpr float4() = default;
    constexpr float4(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr float4(float2 xy, f32 z_, f32 w_) : x(xy.x), y(xy.y), z(z_), w(w_) {}
    constexpr float4(float2 xy, float2 zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
    constexpr float2 xy() const { return {x, y}; }
    constexpr float2 zw() const { return {z, w}; }
    constexpr float4 operator*(f32 s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr float4 operator+(float4 o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
};
static_assert(sizeof(float4) == 16 && alignof(float4) == 16);

struct alignas(8) uint2 {
    u32 x = 0, y = 0;
    constexpr uint2() = default;
    constexpr uint2(u32 x_, u32 y_) : x(x_), y(y_) {}
    constexpr bool operator==(uint2 o) const { return x == o.x && y == o.y; }
};
static_assert(sizeof(uint2) == 8 && alignof(uint2) == 8);

struct alignas(16) uint4 {
    u32 x = 0, y = 0, z = 0, w = 0;
    constexpr uint4() = default;
    constexpr uint4(u32 x_, u32 y_, u32 z_, u32 w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr bool operator==(uint4 o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
};
static_assert(sizeof(uint4) == 16 && alignof(uint4) == 16);

struct int2 {
    i32 x = 0, y = 0;
    constexpr int2() = default;
    constexpr int2(i32 x_, i32 y_) : x(x_), y(y_) {}
    constexpr bool operator==(int2 o) const { return x == o.x && y == o.y; }
};

constexpr f32 kPi = 3.14159265358979323846f;
constexpr f32 kTwoPi = 2.f * kPi;

constexpr f32 dot(float2 a, float2 b) { return a.x * b.x + a.y * b.y; }
constexpr f32 cross(float2 a, float2 b) { return a.x * b.y - a.y * b.x; }
constexpr f32 lengthSq(float2 a) { return dot(a, a); }
inline f32 length(float2 a) { return std::sqrt(lengthSq(a)); }
inline float2 normalize(float2 a) {
    const f32 l = length(a);
    return l > 1e-8f ? a / l : float2{0.f, 0.f};
}
constexpr float2 perp(float2 a) { return {-a.y, a.x}; }
inline float2 fromAngle(f32 rad) { return {std::cos(rad), std::sin(rad)}; }
inline f32 angleOf(float2 a) { return std::atan2(a.y, a.x); }

template <class T> constexpr T lerp(T a, T b, f32 t) { return a + (b - a) * t; }
template <class T> constexpr T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
template <class T> constexpr T minv(T a, T b) { return a < b ? a : b; }
template <class T> constexpr T maxv(T a, T b) { return a > b ? a : b; }
constexpr f32 saturate(f32 v) { return clamp(v, 0.f, 1.f); }
constexpr f32 smoothstep(f32 e0, f32 e1, f32 x) {
    const f32 t = saturate((x - e0) / (e1 - e0));
    return t * t * (3.f - 2.f * t);
}
constexpr u32 alignUp(u32 v, u32 a) { return (v + a - 1) / a * a; }
constexpr usize alignUp(usize v, usize a) { return (v + a - 1) / a * a; }
constexpr u64 alignUp(u64 v, u64 a) { return (v + a - 1) / a * a; }
constexpr bool isPow2(u64 v) { return v != 0 && (v & (v - 1)) == 0; }
constexpr u32 divCeil(u32 a, u32 b) { return (a + b - 1) / b; }

} // namespace lb
