// M1 round-trip microbenchmark kernels (docs/06 §6). The data patterns are mirrored on
// the CPU in src/bench/MicroPattern.h (checksums are verified every iteration).
import layouts;

struct MicroParams
{
    uint4 v;   // x = element count (16 B each), y = iteration, z = read-kernel group count, w reserved
};

RWStructuredBuffer<uint4>   gPayload;   // binding 0: P bytes as uint4 elements
RWStructuredBuffer<uint>    gChecksum;  // binding 1: micro_read writes one partial sum per group
[[vk::push_constant]] ConstantBuffer<MicroParams> gParams;

// Step 1: GPU writes P bytes (1 thread per 16 B, trivial arithmetic).
[shader("compute")]
[numthreads(LB_AGENT_GROUP_SIZE, 1, 1)]
void micro_write(uint3 tid : SV_DispatchThreadID)
{
    uint i = tid.x;
    if (i >= gParams.v.x) return;
    uint it = gParams.v.y;
    gPayload[i] = uint4(i ^ it, i * 2654435761u + it, i + it * 40503u, ~i);
}

// Step 3: GPU reads the P bytes the CPU wrote back. Grid-stride loop over gParams.v.z
// groups; each group writes its partial sum of (x ^ y ^ z ^ w) (integer, order-independent,
// no atomics), and the CPU adds the partials.
groupshared uint sAccum[LB_AGENT_GROUP_SIZE];

[shader("compute")]
[numthreads(LB_AGENT_GROUP_SIZE, 1, 1)]
void micro_read(uint3 tid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    uint count = gParams.v.x;
    uint stride = gParams.v.z * LB_AGENT_GROUP_SIZE;
    uint acc = 0;
    for (uint i = tid.x; i < count; i += stride)
    {
        uint4 p = gPayload[i];
        acc += p.x ^ p.y ^ p.z ^ p.w;
    }
    sAccum[gtid.x] = acc;
    GroupMemoryBarrierWithGroupSync();
    if (gtid.x == 0)
    {
        uint total = 0;
        for (uint j = 0; j < LB_AGENT_GROUP_SIZE; ++j) total += sAccum[j];
        gChecksum[gid.x] = total;
    }
}
