#ifndef EITHER_H
#define EITHER_H

// Make expected<T, E> behave like the Either<E> monad when used in a coroutine.

#include "expected.h"
#include "return_object_holder.h"

#include <experimental/coroutine>

template <typename T, typename E>
struct either_awaitable;

template <typename T, typename E>
struct either_promise {
  return_object_holder<expected<T, E>>* data;

  auto get_return_object() { return make_return_object_holder(data); }
  auto initial_suspend() { return std::experimental::suspend_never{}; }
  auto final_suspend() { return std::experimental::suspend_never{}; }

  template <typename U>
  auto await_transform(expected<U, E> e) {
    return either_awaitable<U, E>{std::move(e)};
  }

  void return_value(T x) { data->emplace(std::move(x)); }
  // void return_value(E x) { *data = std::move(x); }
  void unhandled_exception() {}
};

// This makes expected<T, E> useable as a coroutine return type.
template <typename T, typename E, typename... Args>
struct std::experimental::coroutine_traits<expected<T, E>, Args...> {
  using promise_type = either_promise<T, E>;
};

template <typename T, typename E>
struct either_awaitable {
  expected<T, E> e;
  constexpr auto await_ready() const noexcept { return e.good(); }
  constexpr auto await_resume() noexcept { return std::move(e.value()); }

  template <typename U>
  constexpr void await_suspend(
      std::experimental::coroutine_handle<either_promise<U, E>> h) {
    h.promise().data->emplace(std::move(e.error()));
    h.destroy();
  }
};

#endif  // EITHER_H
