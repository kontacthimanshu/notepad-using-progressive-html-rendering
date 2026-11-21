#pragma once
// Expected<T, E>: value-or-error return type used across all module boundaries.
// Uses std::expected when the standard library provides it; otherwise a minimal polyfill.

#include <type_traits>
#include <utility>
#include <variant>

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>
namespace pxc::core {
template <class T, class E> using Expected = std::expected<T, E>;
template <class E> using Unexpected = std::unexpected<E>;
} // namespace pxc::core
#else
namespace pxc::core {

template <class E> class Unexpected {
public:
    explicit Unexpected(E e) : error_(std::move(e)) {}
    const E& error() const& noexcept { return error_; }
    E& error() & noexcept { return error_; }
    E&& error() && noexcept { return std::move(error_); }

private:
    E error_;
};
template <class E> Unexpected(E) -> Unexpected<E>;

template <class T, class E> class Expected {
public:
    Expected() requires std::is_default_constructible_v<T> : v_(std::in_place_index<0>) {}
    Expected(const T& v) : v_(std::in_place_index<0>, v) {}
    Expected(T&& v) : v_(std::in_place_index<0>, std::move(v)) {}
    template <class G> Expected(Unexpected<G> u) : v_(std::in_place_index<1>, std::move(u).error()) {}

    bool has_value() const noexcept { return v_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }

    T& value() & { return std::get<0>(v_); }
    const T& value() const& { return std::get<0>(v_); }
    T&& value() && { return std::get<0>(std::move(v_)); }
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(*this).value(); }
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

    E& error() & { return std::get<1>(v_); }
    const E& error() const& { return std::get<1>(v_); }
    E&& error() && { return std::get<1>(std::move(v_)); }

private:
    std::variant<T, E> v_;
};

template <class E> class Expected<void, E> {
public:
    Expected() = default;
    template <class G> Expected(Unexpected<G> u) : ok_(false), error_(std::move(u).error()) {}
    bool has_value() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    void value() const {}
    const E& error() const& { return error_; }
    E& error() & { return error_; }

private:
    bool ok_ = true;
    E error_{};
};

} // namespace pxc::core
#endif
