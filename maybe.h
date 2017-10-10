#ifndef MAYBE_H
#define MAYBE_H

// Make std::optional behave like the Maybe monad when used in a coroutine.

#include "return_object_holder.h"

#include <experimental/coroutine>
#include <optional>

template <typename T>
struct maybe_promise {
  return_object_holder<std::optional<T>>* data;

  auto get_return_object() { return make_return_object_holder(data); }
  auto initial_suspend() { return std::experimental::suspend_never{}; }
  auto final_suspend() { return std::experimental::suspend_never{}; }

  void return_value(T x) { data->emplace(std::move(x)); }
  void unhandled_exception() {}
};

// This makes std::optional<T> useable as a coroutine return type. Strictly, this
// specilaization should depend on a user-defined type, otherwise this is undefined
// behaviour. As this is purely for demonstration purposes, let's live dangerously.
namespace std::experimental {
  template <typename T, typename... Args>
  struct coroutine_traits<std::optional<T>, Args...> {
    using promise_type = ::maybe_promise<T>;
  };
}  // namespace std::experimental

template <typename T>
struct maybe_awaitable {
  std::optional<T> o;
  auto await_ready() { return o.has_value(); }
  auto await_resume() { return *o; }

  template <typename U>
  void await_suspend(std::experimental::coroutine_handle<maybe_promise<U>> h) {
    h.promise().data->emplace(std::nullopt);
    h.destroy();
  }
};

template <typename T>
auto operator co_await(std::optional<T> o) {
  return maybe_awaitable<T>{std::move(o)};
}

#endif  // MAYBE_H
