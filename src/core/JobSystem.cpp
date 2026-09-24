#include "core/JobSystem.h"

#include "core/Log.h"

namespace lb {

JobSystem::JobSystem(u32 workerCount) {
    m_threads.reserve(workerCount);
    for (u32 i = 0; i < workerCount; ++i) {
        m_threads.emplace_back([this, i] { workerMain(i + 1); });
    }
}

JobSystem::~JobSystem() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quit.store(true, std::memory_order_release);
    }
    m_cv.notify_all();
    for (auto& t : m_threads) t.join();
}

void JobSystem::runChunks(Job& job, u32 workerIndex) {
    for (;;) {
        const u32 chunk = job.nextChunk.fetch_add(1, std::memory_order_acq_rel);
        if (chunk >= job.chunkCount) return;
        const u32 begin = chunk * job.chunkSize;
        const u32 end = (begin + job.chunkSize < job.count) ? begin + job.chunkSize : job.count;
        (*job.fn)(chunk, begin, end, workerIndex);
        job.finished.fetch_add(1, std::memory_order_acq_rel);
    }
}

void JobSystem::workerMain(u32 workerIndex) {
    u64 seenGeneration = 0;
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&] {
                return m_quit.load(std::memory_order_acquire) ||
                       m_generation.load(std::memory_order_acquire) != seenGeneration;
            });
            if (m_quit.load(std::memory_order_acquire)) return;
            seenGeneration = m_generation.load(std::memory_order_acquire);
        }
        runChunks(m_job, workerIndex);
        // Wake the caller in case we ran the last chunk. Taking the mutex (even empty)
        // orders this notify after the caller's predicate check, so no wakeup is lost.
        if (m_job.finished.load(std::memory_order_acquire) >= m_job.chunkCount) {
            { std::lock_guard<std::mutex> lock(m_mutex); }
            m_doneCv.notify_one();
        }
    }
}

void JobSystem::parallelFor(u32 count, u32 chunkSize, ChunkFn fn) {
    if (count == 0) return;
    if (chunkSize == 0) chunkSize = 1;
    const u32 chunkCount = (count + chunkSize - 1) / chunkSize;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_job.fn = &fn;
        m_job.count = count;
        m_job.chunkSize = chunkSize;
        m_job.chunkCount = chunkCount;
        m_job.nextChunk.store(0, std::memory_order_release);
        m_job.finished.store(0, std::memory_order_release);
        m_generation.fetch_add(1, std::memory_order_acq_rel);
    }
    if (chunkCount > 1 && !m_threads.empty()) m_cv.notify_all();

    runChunks(m_job, 0);

    // Wait for chunks still running on workers.
    if (m_job.finished.load(std::memory_order_acquire) < chunkCount) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_doneCv.wait(lock, [&] { return m_job.finished.load(std::memory_order_acquire) >= chunkCount; });
    }
    m_job.fn = nullptr;
}

void JobSystem::parallelForEach(u32 count, FunctionRef<void(u32, u32)> fn) {
    parallelFor(count, 1, [&](u32 chunk, u32, u32, u32 worker) { fn(chunk, worker); });
}

} // namespace lb
