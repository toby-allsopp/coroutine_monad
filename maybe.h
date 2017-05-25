#ifndef MAYBE_H
#define MAYBE_H

// Make std::optional behave like the Maybe monad when used in a coroutine.

#include <experimental/coroutine>
#include <optional>

template <typename T>
struct return_object_holder {
  // The staging object that is returned (by copy/move) to the caller of the coroutine.
  T stage;

  // When constructed, we construct the staging object by forwarding the args and then
  // assign a pointer to it to the supplied reference to pointer.
  template <typename... Args>
  return_object_holder(T*& p, Args&&... args) : stage{std::forward<Args>(args)...} {
    p = &stage;
  }
  return_object_holder(return_object_holder const&) {
    // A non-trivial copy constructor is required to ensure that an object of this class
    // is not returned via registers, as that can result in the value of `this` being
    // different in our constructor than later on. Note that deleting the copy constructor
    // is not sufficient until all compilers implement the updated wording in
    // https://wg21.link/p0135r1.
    throw std::logic_error("return object must not be copied!");
  }

  // We assume that we will be converted only once, so we can move from the staging
  // object.
  operator T() { return std::move(stage); }
};

template <typename T>
auto make_return_object_holder(T*& p) {
  return return_object_holder<T>{p};
}

template <typename T>
struct maybe_promise {
  std::optional<T>* data;

  auto get_return_object() { return make_return_object_holder(data); }
  auto initial_suspend() { return std::experimental::suspend_never{}; }
  auto final_suspend() { return std::experimental::suspend_never{}; }

  void return_value(T x) { *data = std::move(x); }
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
  auto await_resume() { return o.value(); }

  template <typename U>
  void await_suspend(std::experimental::coroutine_handle<maybe_promise<U>> h) {
    h.promise().data->reset();
    h.destroy();
  }
};

template <typename T>
auto operator co_await(std::optional<T> o) {
  return maybe_awaitable<T>{std::move(o)};
}

#endif  // MAYBE_H
