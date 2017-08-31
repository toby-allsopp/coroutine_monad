#ifndef EITHER_H
#define EITHER_H

// Make expected<T, E> behave like the Either<E> monad when used in a coroutine.

#include "expected.h"
#include "monad_promise.h"

template <typename T, typename E>
struct monad_traits<expected<T, E>> {
  using value_type = T;

  template <typename U>
  using rebind = expected<U, E>;

  static auto pure(T x) { return expected<T, E>{std::move(x)}; }

  template <typename F>
  static auto fmap(expected<T, E> const& e, F&& f)
      -> expected<std::result_of_t<F(T)>, E> {
    if (e.good()) return f(e.value());
    return e.error();
  }

  template <typename F>
  static auto bind(expected<T, E> const& e, F&& f) -> std::result_of_t<F(T)> {
    if (e.good()) return f(e.value());
    return {e.error()};
  }
};

// This makes expected<T, E> useable as a coroutine return type.
template <typename T, typename E, typename... Args>
struct std::experimental::coroutine_traits<expected<T, E>, Args...> {
  using promise_type = monad_promise<expected<T, E>>;
};

#endif  // EITHER_H
