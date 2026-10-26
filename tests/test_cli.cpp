#include "app/Cli.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace lb::app;

namespace {
ParseStatus parse(std::vector<const char*> args, Options& o, std::string& err) {
    args.insert(args.begin(), "lightbound");
    return parseCommandLine(static_cast<int>(args.size()), args.data(), o, err);
}
} // namespace

TEST_CASE("CLAUDE.md sample command lines parse") {
    Options o;
    std::string err;
    REQUIRE(parse({"--mode=play", "--seed=1234"}, o, err) == ParseStatus::Ok);
    CHECK(o.mode == Mode::Play);
    CHECK(o.seed == 1234u);
    CHECK(o.seedExplicit);

    o = Options{};
    REQUIRE(parse({"--mode=microbench", "--out=results/micro.csv"}, o, err) == ParseStatus::Ok);
    CHECK(o.mode == Mode::Microbench);
    CHECK(o.out == "results/micro.csv");
    CHECK_FALSE(o.resume);

    o = Options{};
    REQUIRE(parse({"--mode=microbench", "--resume", "--out=results/micro.csv"}, o, err) == ParseStatus::Ok);
    CHECK(o.resume);
    CHECK(parse({"--resume=on"}, o, err) == ParseStatus::Error);  // takes no value

    o = Options{};
    REQUIRE(parse({"--mode=bench", "--strategy=S2", "--k=2", "--agents=20000", "--seed=101",
                   "--frames=3600", "--out=results/run.csv"}, o, err) == ParseStatus::Ok);
    CHECK(o.mode == Mode::Bench);
    CHECK(o.strategy == Strategy::S2);
    CHECK(o.k == 2u);
    CHECK(o.agents == 20000u);
    CHECK(o.seed == 101u);
    CHECK(o.frames == 3600u);
}

TEST_CASE("all protocol §11 flags are accepted in both syntaxes") {
    Options o;
    std::string err;
    REQUIRE(parse({"--mode", "audit", "--strategy", "s1", "--k", "4", "--delay=1", "--agents", "100000",
                   "--seed=0xDEADBEEF", "--frames", "10", "--warmup", "5", "--submit=perpass",
                   "--cpuwait", "block", "--threads=8", "--memvariant=hostcached", "--audit=on",
                   "--audio=null", "--preset=quick", "--out", "x.csv", "--tag", "hello world",
                   "--power=on", "--vsync=off", "--validation=on", "--width=640", "--height=360",
                   "--exit-after=3", "--log=warn"}, o, err) == ParseStatus::Ok);
    CHECK(o.mode == Mode::Audit);
    CHECK(o.strategy == Strategy::S1);
    CHECK(o.k == 4u);
    CHECK(o.delay == 1u);
    CHECK(o.agents == 100000u);
    CHECK(o.seed == 0xDEADBEEFu);
    CHECK(o.frames == 10u);
    CHECK(o.warmup == 5u);
    CHECK(o.submit == Submit::PerPass);
    CHECK(o.cpuwait == CpuWait::Block);
    CHECK(o.threads == 8u);
    CHECK(o.memvariant == MemVariant::HostCached);
    CHECK(o.audit);
    CHECK(o.audio == AudioMode::Null);
    CHECK(o.preset == Preset::Quick);
    CHECK(o.out == "x.csv");
    CHECK(o.tag == "hello world");
    CHECK(o.power);
    CHECK_FALSE(o.vsync);
    CHECK(o.vsyncExplicit);
    CHECK(o.validation);
    CHECK(o.width == 640u);
    CHECK(o.height == 360u);
    CHECK(o.exitAfter == 3u);
    CHECK(o.logLevel == 2);
}

TEST_CASE("bad input is rejected with a message") {
    Options o;
    std::string err;
    CHECK(parse({"--bogus=1"}, o, err) == ParseStatus::Error);
    CHECK(err.find("bogus") != std::string::npos);
    CHECK(parse({"--k=5"}, o, err) == ParseStatus::Error);
    CHECK(parse({"--k=0"}, o, err) == ParseStatus::Error);
    CHECK(parse({"--strategy=S9"}, o, err) == ParseStatus::Error);
    CHECK(parse({"--agents=abc"}, o, err) == ParseStatus::Error);
    CHECK(parse({"--agents=200000"}, o, err) == ParseStatus::Error);
    CHECK(parse({"--seed"}, o, err) == ParseStatus::Error);
    CHECK(parse({"positional"}, o, err) == ParseStatus::Error);
    CHECK(parse({"--help=1"}, o, err) == ParseStatus::Error);
    CHECK(parse({"--help"}, o, err) == ParseStatus::Help);
}

TEST_CASE("normalizeOptions applies protocol rules") {
    Options o;
    std::string err;
    REQUIRE(parse({"--mode=bench", "--strategy=S0", "--k=3"}, o, err) == ParseStatus::Ok);
    const std::string note = normalizeOptions(o);
    CHECK(o.k == 1u);
    CHECK_FALSE(note.empty());
    CHECK_FALSE(o.vsync); // bench defaults to vsync off

    o = Options{};
    REQUIRE(parse({"--mode=play"}, o, err) == ParseStatus::Ok);
    normalizeOptions(o);
    CHECK(o.vsync);

    o = Options{};
    REQUIRE(parse({"--mode=bench", "--vsync=on"}, o, err) == ParseStatus::Ok);
    normalizeOptions(o);
    CHECK(o.vsync);

    o = Options{};
    REQUIRE(parse({"--mode=audit"}, o, err) == ParseStatus::Ok);
    normalizeOptions(o);
    CHECK(o.audit);
}

TEST_CASE("--micro-gpuload selects the hot-GPU cells (DECISIONS #32)") {
    Options o;
    std::string err;
    REQUIRE(parse({"--mode=microbench"}, o, err) == ParseStatus::Ok);
    CHECK(o.microGpuLoad == GpuLoad::Idle);
    CHECK(describeOptions(o).find("micro-gpuload") == std::string::npos);  // default: header unchanged
    REQUIRE(parse({"--mode=microbench", "--micro-gpuload=hot"}, o, err) == ParseStatus::Ok);
    CHECK(o.microGpuLoad == GpuLoad::Hot);
    CHECK(describeOptions(o).find("micro-gpuload=hot") != std::string::npos);
    REQUIRE(parse({"--micro-gpuload", "both"}, o, err) == ParseStatus::Ok);
    CHECK(o.microGpuLoad == GpuLoad::Both);
    CHECK(parse({"--micro-gpuload=warm"}, o, err) == ParseStatus::Error);
    CHECK(std::string(cliName(GpuLoad::Both)) == "both");
}

TEST_CASE("--micro-qos requests the bench thread's QoS class (DECISIONS #36)") {
    Options o;
    std::string err;
    REQUIRE(parse({"--mode=microbench"}, o, err) == ParseStatus::Ok);
    CHECK(o.microQos == ThreadQos::Default);
    CHECK(describeOptions(o).find("micro-qos") == std::string::npos);  // default: header unchanged
    REQUIRE(parse({"--mode=microbench", "--micro-qos=interactive"}, o, err) == ParseStatus::Ok);
    CHECK(o.microQos == ThreadQos::Interactive);
    CHECK(describeOptions(o).find("micro-qos=interactive") != std::string::npos);
    REQUIRE(parse({"--micro-qos", "default"}, o, err) == ParseStatus::Ok);
    CHECK(o.microQos == ThreadQos::Default);
    REQUIRE(parse({"--micro-qos=high"}, o, err) == ParseStatus::Ok);
    CHECK(o.microQos == ThreadQos::High);
    CHECK(describeOptions(o).find("micro-qos=high") != std::string::npos);
    CHECK(parse({"--micro-qos=realtime"}, o, err) == ParseStatus::Error);
}

TEST_CASE("--micro-cpuload sets the CPU companion load (DECISIONS #36)") {
    Options o;
    std::string err;
    REQUIRE(parse({"--mode=microbench"}, o, err) == ParseStatus::Ok);
    CHECK(o.microCpuLoad == 0);
    CHECK(describeOptions(o).find("micro-cpuload") == std::string::npos);  // default: header unchanged
    Options l;
    REQUIRE(parse({"--mode=microbench", "--micro-cpuload=2"}, l, err) == ParseStatus::Ok);
    CHECK(l.microCpuLoad == 2);
    CHECK(describeOptions(l).find("micro-cpuload=2") != std::string::npos);
    Options bad;
    CHECK(parse({"--micro-cpuload=17"}, bad, err) == ParseStatus::Error);
    CHECK(parse({"--micro-cpuload=two"}, bad, err) == ParseStatus::Error);
}

TEST_CASE("describeOptions round-trips key fields") {
    Options o;
    std::string err;
    REQUIRE(parse({"--mode=bench", "--strategy=S3", "--agents=5000", "--tag=t1"}, o, err) == ParseStatus::Ok);
    const std::string d = describeOptions(o);
    CHECK(d.find("mode=bench") != std::string::npos);
    CHECK(d.find("strategy=S3") != std::string::npos);
    CHECK(d.find("agents=5000") != std::string::npos);
    CHECK(d.find("tag=t1") != std::string::npos);
}
