// Not a real pass. References every shared struct so that slangc's reflection
// emits their layouts; consumed by tools/gen_layouts.py (see cmake/LbSlang.cmake).
import layouts;

RWStructuredBuffer<Perception>    gPerception;
RWStructuredBuffer<Intent>        gIntent;
RWStructuredBuffer<RefineRequest> gRefineReq;
RWStructuredBuffer<RefineResult>  gRefineRes;
RWStructuredBuffer<GpuEvents>     gEvents;
ConstantBuffer<FrameConsts>       gConsts;

[shader("compute")]
[numthreads(LB_AGENT_GROUP_SIZE, 1, 1)]
void layout_probe(uint3 tid : SV_DispatchThreadID)
{
    uint i = tid.x;
    gIntent[i].desired_speed = gPerception[i].lit + gConsts.misc.x;
    gRefineRes[i].agent = gRefineReq[i].agent + gEvents[0].count + gEvents[0].ev[i % LB_GPU_EVENT_CAPACITY].kind;
}
