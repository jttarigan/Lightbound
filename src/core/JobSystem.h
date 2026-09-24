#pragma once
// Job system with a fixed number of worker threads W (docs/05 §1). Used for pass B/D.
// parallelFor splits [0, count) into chunks of chunkSize; chunk indices are stable so
// per-chunk outputs can be merged deterministically (R4). The calling thread
// participates as worker 0; workers are 1..W. No allocation per call.
#include "core/FunctionRef.h"
#include "core/Types.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace lb {

class JobSystem {
public:
    /// fn(chunkIndex, begin, end, workerIndex)
    using ChunkFn = FunctionRef<void(u32, u32, u32, u32)>;

    explicit JobSystem(u32 workerCount);
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    /// Number of worker threads (excluding the calling thread).
    u32 workerCount() const { return static_cast<u32>(m_threads.size()); }
    /// Worker index space size: workerCount() + 1 (index 0 = caller).
    u32 workerSlots() const { return workerCount() + 1; }

    /// Blocks until all chunks have run. Must not be called re-entrantly.
    void parallelFor(u32 count, u32 chunkSize, ChunkFn fn);

    /// Convenience: one chunk per item. fn(itemIndex, workerIndex).
    void parallelForEach(u32 count, FunctionRef<void(u32, u32)> fn);

private:
    struct Job {
        const ChunkFn* fn = nullptr;
        u32 count = 0;
        u32 chunkSize = 0;
        u32 chunkCount = 0;
        std::atomic<u32> nextChunk{0};
        std::atomic<u32> finished{0};
    };

    void workerMain(u32 workerIndex);
    void runChunks(Job& job, u32 workerIndex);

    std::vector<std::thread> m_threads;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::condition_variable m_doneCv;
    Job m_job;
    std::atomic<u64> m_generation{0};
    std::atomic<bool> m_quit{false};
};

} // namespace lb
