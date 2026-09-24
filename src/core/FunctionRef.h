#pragma once
// Non-owning callable reference (no allocation, unlike std::function).
#include <type_traits>
#include <utility>

namespace lb {

template <class Sig> class FunctionRef;

template <class R, class... Args>
class FunctionRef<R(Args...)> {
public:
    FunctionRef() = delete;

    template <class F,
              class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, FunctionRef> &&
                                       std::is_invocable_r_v<R, F&, Args...>>>
    FunctionRef(F&& f) noexcept // NOLINT(google-explicit-constructor)
        : m_obj(const_cast<void*>(static_cast<const void*>(&f))),
          m_call(&invoke<std::remove_reference_t<F>>) {}

    R operator()(Args... args) const { return m_call(m_obj, std::forward<Args>(args)...); }

private:
    template <class F> static R invoke(void* obj, Args... args) {
        return (*static_cast<F*>(obj))(std::forward<Args>(args)...);
    }
    void* m_obj;
    R (*m_call)(void*, Args...);
};

} // namespace lb
