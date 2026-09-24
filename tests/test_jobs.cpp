#include "core/JobSystem.h"

#include <doctest/doctest.h>

#include <atomic>
#include <vector>

using namespace lb;

TEST_CASE("parallelFor visits every index exactly once, with stable chunk indices") {
    JobSystem jobs(4);
    CHECK(jobs.workerCount() == 4);
    CHECK(jobs.workerSlots() == 5);

    const u32 count = 100003;
    const u32 chunk = 1024;
    const u32 chunkCount = (count + chunk - 1) / chunk;
    std::vector<std::atomic<u32>> hits(count);
    for (auto& h : hits) h.store(0);
    std::vector<u64> chunkSums(chunkCount, 0);
    std::vector<u32> chunkWorker(chunkCount, 0xFFFFFFFFu);

    jobs.parallelFor(count, chunk, [&](u32 c, u32 begin, u32 end, u32 worker) {
        REQUIRE(c < chunkCount);
        REQUIRE(begin == c * chunk);
        REQUIRE(end <= count);
        REQUIRE(worker < jobs.workerSlots());
        u64 sum = 0;
        for (u32 i = begin; i < end; ++i) {
            hits[i].fetch_add(1);
            sum += i;
        }
        chunkSums[c] = sum;
        chunkWorker[c] = worker;
    });

    for (u32 i = 0; i < count; ++i) REQUIRE(hits[i].load() == 1);
    u64 total = 0;
    for (u64 s : chunkSums) total += s;
    CHECK(total == static_cast<u64>(count) * (count - 1) / 2);
    for (u32 w : chunkWorker) CHECK(w != 0xFFFFFFFFu);
}

TEST_CASE("parallelFor is deterministic when merged in chunk order and survives many rounds") {
    JobSystem jobs(3);
    const u32 count = 5000;
    std::vector<u32> first(count);
    for (int round = 0; round < 200; ++round) {
        std::vector<u32> out(count, 0);
        jobs.parallelFor(count, 37, [&](u32, u32 begin, u32 end, u32) {
            for (u32 i = begin; i < end; ++i) out[i] = i * 2654435761u;
        });
        if (round == 0) first = out;
        else REQUIRE(out == first);
    }
}

TEST_CASE("parallelFor edge cases") {
    JobSystem jobs(2);
    std::atomic<u32> calls{0};
    jobs.parallelFor(0, 16, [&](u32, u32, u32, u32) { calls.fetch_add(1); });
    CHECK(calls.load() == 0);
    jobs.parallelFor(5, 100, [&](u32 c, u32 b, u32 e, u32) {
        CHECK(c == 0);
        CHECK(b == 0);
        CHECK(e == 5);
        calls.fetch_add(1);
    });
    CHECK(calls.load() == 1);
    jobs.parallelForEach(10, [&](u32, u32) { calls.fetch_add(1); });
    CHECK(calls.load() == 11);

    JobSystem single(0);
    single.parallelFor(1000, 10, [&](u32, u32, u32, u32 w) { CHECK(w == 0); calls.fetch_add(1); });
    CHECK(calls.load() == 111);
}
