#pragma once
// Linear (bump) allocator over one preallocated block. The frame loop must not
// allocate; systems reserve arenas at startup and reset them per frame/floor.
#include "core/Math.h"
#include "core/Types.h"

#include <cstdlib>
#include <new>

namespace lb {

class Arena {
public:
    struct Marker { usize offset; };

    Arena() = default;
    explicit Arena(usize capacityBytes) { reserve(capacityBytes); }
    ~Arena() { release(); }
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&& o) noexcept { moveFrom(o); }
    Arena& operator=(Arena&& o) noexcept {
        if (this != &o) { release(); moveFrom(o); }
        return *this;
    }

    /// (Re)allocates the backing block. Not for use in the frame loop.
    bool reserve(usize capacityBytes) {
        release();
        if (capacityBytes == 0) return true;
        m_base = static_cast<u8*>(::operator new(capacityBytes, std::align_val_t{kBaseAlign}, std::nothrow));
        if (m_base == nullptr) return false;
        m_capacity = capacityBytes;
        m_offset = 0;
        return true;
    }

    void release() {
        if (m_base != nullptr) ::operator delete(m_base, std::align_val_t{kBaseAlign});
        m_base = nullptr;
        m_capacity = 0;
        m_offset = 0;
        m_highWater = 0;
    }

    /// Returns nullptr (and leaves the arena untouched) when out of space.
    void* alloc(usize bytes, usize align = alignof(std::max_align_t)) {
        if (align == 0 || !isPow2(align)) return nullptr;
        const usize start = alignUp(m_offset, align);
        if (bytes > m_capacity || start > m_capacity - bytes) return nullptr;
        m_offset = start + bytes;
        if (m_offset > m_highWater) m_highWater = m_offset;
        return m_base + start;
    }

    /// Uninitialised array of trivially-constructible T. nullptr when out of space.
    template <class T> T* allocArray(usize count) {
        return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }

    /// Zero-initialised array.
    template <class T> T* allocArrayZeroed(usize count) {
        T* p = allocArray<T>(count);
        if (p != nullptr) {
            u8* b = reinterpret_cast<u8*>(p);
            for (usize i = 0; i < sizeof(T) * count; ++i) b[i] = 0;
        }
        return p;
    }

    Marker mark() const { return Marker{m_offset}; }
    void resetTo(Marker m) { if (m.offset <= m_offset) m_offset = m.offset; }
    void reset() { m_offset = 0; }

    usize used() const { return m_offset; }
    usize capacity() const { return m_capacity; }
    usize highWater() const { return m_highWater; }
    usize remaining() const { return m_capacity - m_offset; }
    bool owns(const void* p) const {
        const u8* b = static_cast<const u8*>(p);
        return b >= m_base && b < m_base + m_capacity;
    }

private:
    static constexpr usize kBaseAlign = 64;
    void moveFrom(Arena& o) {
        m_base = o.m_base; m_capacity = o.m_capacity; m_offset = o.m_offset; m_highWater = o.m_highWater;
        o.m_base = nullptr; o.m_capacity = 0; o.m_offset = 0; o.m_highWater = 0;
    }
    u8* m_base = nullptr;
    usize m_capacity = 0;
    usize m_offset = 0;
    usize m_highWater = 0;
};

} // namespace lb
