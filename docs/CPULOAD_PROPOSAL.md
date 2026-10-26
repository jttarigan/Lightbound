# CPU-load condition for the microbenchmark: draft for the user's decision (STATUS to-do 2)

_Drafted 2026-10-26 (session 21). Nothing here is applied: docs/06 changes and DECISIONS #37 follow the user's choice._

**Why.** The 10-25 clock trace (`results/m1_mac_40w_check/` README, last section) shows that the M5's round trips depend on
the CPU-side load: alone, the spin-waiting bench thread runs at ≈ 2.7 GHz and spin cells are +65…+268 µs above 09-24;
beside two busy threads the cluster runs 4.3–4.5 GHz, spin cells return to 09-24 (±34 µs, three 4K cells +80…+88), and
block cells get slower (4K +85…+96 µs). docs/06 §2 says only "no other apps", which leaves the load to chance.

**Ready in code (2026-10-26; default off, so the protocol is unchanged until the choice):**
- `--micro-cpuload=N`: N busy threads in the bench process (dependent integer chain, no memory traffic), same code on
  Metal and Vulkan (R1). Header `cpuload`, `cpuload.note`, footer `# cpuload_end:` (chain rate ≈ clock proxy).
- `tools/run_microbench.py --cond NAME=ARGS` (repeatable): named conditions alternate run by run, one dir each.
- `tools/m1_mac_ext.sh <a|b|b2> <frozen dir>`: the Mac ext series with LAB.md gates and per-minute logging.
- Still to verify in the first slot (≈ 3 min, needs the root waiter again for the clock): that the in-process load
  lifts the clock like 2× `yes` did (`yes` loops on write syscalls; the chain is pure ALU).

**Draft docs/06 §2 bullet, per option** (replaces "Mac: plugged in, no other apps, …" for the microbenchmark):
- (a) *Idle:* "Microbenchmark: no companion load (`--micro-cpuload=0`). On the M5 the governor then runs a lone
  spin-waiting thread at ≈ 2.7 GHz; spin-wait cells are reported as load-dependent (§12.2 item 5)." Re-measure the
  idle reference on the Mac only; the PC series stand.
- (b) *Loaded:* "Microbenchmark: two companion threads (`--micro-cpuload=2`) on both platforms, so the bench thread
  never runs on an otherwise idle SoC (the game's simulation never does)." Mac and PC re-measure (M1 protocol + ext).
- (b′) *Both:* "Microbenchmark: two named conditions, idle (`--micro-cpuload=0`) and loaded (`=2`), alternated run by
  run on both platforms; the paper reports both and the load sensitivity." Cost: Mac ext ≈ 140 min instead of 70;
  PC ext ≈ 2× its ≈ 3 h; the PC needs this code merged and rebuilt first.

**Recommendation: (b′).** The load moves spin and block cells in opposite directions on the M5, so a single condition
hides half of the effect; on the PC (spin and block within 6 µs) it is expected to change little, which is itself a
platform difference worth one table.
