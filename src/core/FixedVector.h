#pragma once
// Fixed-capacity vector with inline storage; never allocates. push_back returns false
// when full (callers in the frame loop must handle that; nothing throws).
#include "core/Types.h"

#include <new>
#include <type_traits>
#include <utility>

namespace lb {

template <class T, usize N>
class FixedVector {
public:
    static_assert(N > 0);
    FixedVector() = default;
    ~FixedVector() { clear(); }
    FixedVector(const FixedVector& o) { for (usize i = 0; i < o.m_size; ++i) push_back(o[i]); }
    FixedVector& operator=(const FixedVector& o) {
        if (this != &o) { clear(); for (usize i = 0; i < o.m_size; ++i) push_back(o[i]); }
        return *this;
    }

    static constexpr usize capacity() { return N; }
    usize size() const { return m_size; }
    bool empty() const { return m_size == 0; }
    bool full() const { return m_size == N; }

    bool push_back(const T& v) {
        if (m_size == N) return false;
        new (ptr(m_size)) T(v);
        ++m_size;
        return true;
    }
    bool push_back(T&& v) {
        if (m_size == N) return false;
        new (ptr(m_size)) T(std::move(v));
        ++m_size;
        return true;
    }
    template <class... Args> T* emplace_back(Args&&... args) {
        if (m_size == N) return nullptr;
        T* p = new (ptr(m_size)) T(std::forward<Args>(args)...);
        ++m_size;
        return p;
    }
    void pop_back() {
        if (m_size == 0) return;
        --m_size;
        ptr(m_size)->~T();
    }
    void clear() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (usize i = 0; i < m_size; ++i) ptr(i)->~T();
        }
        m_size = 0;
    }
    /// Resize with default-constructed elements; false if n > N.
    bool resize(usize n) {
        if (n > N) return false;
        while (m_size > n) pop_back();
        while (m_size < n) { new (ptr(m_size)) T(); ++m_size; }
        return true;
    }

    T& operator[](usize i) { return *ptr(i); }
    const T& operator[](usize i) const { return *ptr(i); }
    T& back() { return *ptr(m_size - 1); }
    const T& back() const { return *ptr(m_size - 1); }
    T* data() { return ptr(0); }
    const T* data() const { return ptr(0); }
    T* begin() { return ptr(0); }
    T* end() { return ptr(m_size); }
    const T* begin() const { return ptr(0); }
    const T* end() const { return ptr(m_size); }

private:
    T* ptr(usize i) { return reinterpret_cast<T*>(m_storage) + i; }
    const T* ptr(usize i) const { return reinterpret_cast<const T*>(m_storage) + i; }
    alignas(T) unsigned char m_storage[N * sizeof(T)];
    usize m_size = 0;
};

} // namespace lb
