#ifndef MONAD_PROMISE_H
#define MONAD_PROMISE_H

#include "return_object_holder.h"

#include <experimental/coroutine>
#include <iostream>
#include <tuple>
#include <vector>

template <typename T>
struct monad_traits;

template <typename M, typename F>
auto fmap(M&& m, F&& f) {
  return monad_traits<std::remove_cv_t<std::remove_reference_t<M>>>::fmap(
      std::forward<M>(m), std::forward<F>(f));
}

template <typename M, typename F>
auto operator>>=(M&& m, F&& f) {
  return monad_traits<std::remove_cv_t<std::remove_reference_t<M>>>::bind(
      std::forward<M>(m), std::forward<F>(f));
}

template <typename M, typename A>
auto pure(A&& a) {
  return monad_traits<M>::pure(std::forward<A>(a));
}

template <typename M, typename U>
using monad_rebind_t = typename monad_traits<M>::template rebind<U>;

template <typename P>
struct shared_coroutine_handle {
  using handle_type = std::experimental::coroutine_handle<P>;
  handle_type h = nullptr;

  shared_coroutine_handle() = default;
  shared_coroutine_handle(handle_type h) : h(h) {
    // Note that we don't increment the reference count stored in h.promise() here. This
    // is because of the way the promise itself holds a shared_coroutine_handle to itself.
  }
  shared_coroutine_handle(shared_coroutine_handle const& o) : shared_coroutine_handle() {
    *this = o;
    std::cout << this << ": shared_coroutine_handle copy constructor: h = " << h.address()
              << std::endl;
  }
  shared_coroutine_handle(shared_coroutine_handle&& o) {
    h = std::exchange(o.h, nullptr);
    std::cout << this << ": shared_coroutine_handle move constructor: h = " << h.address()
              << std::endl;
  }

  shared_coroutine_handle& operator=(shared_coroutine_handle const& o) {
    reset();
    h = o.h;
    if (h) h.promise().inc_ref();
    return *this;
  }
  shared_coroutine_handle& operator=(shared_coroutine_handle&& o) {
    reset();
    h = std::exchange(o.h, nullptr);
    return *this;
  }

  ~shared_coroutine_handle() {
    std::cout << this << ": ~shared_coroutine_handle: h = " << h.address() << std::endl;
    reset();
  }

  void reset() {
    auto h2 = std::exchange(h, nullptr);
    if (h2) h2.promise().dec_ref();
  }
};

template <typename M>
struct monad_awaitable;

template <typename M>
struct monad_promise {
  using handle_type = std::experimental::coroutine_handle<monad_promise>;
  return_object_holder<M>* data;
  std::vector<std::optional<M>*> bind_return_storage;
  int ref_count = 0;
  shared_coroutine_handle<monad_promise> sch{handle_type::from_promise(*this)};
  int susp_count = 0;

  ~monad_promise() { std::cout << this << ": ~monad_promise" << std::endl; }

  void push_storage(std::optional<M>& storage) {
    bind_return_storage.push_back(&storage);
  }

  template <typename... Args>
  void emplace_value(Args&&... args) {
    auto storage = bind_return_storage.back();
    if (storage != &data->stage) {
      std::cout << this << ": setting bind_return_storage" << std::endl;
    } else {
      std::cout << this << ": setting stage" << std::endl;
    }
    storage->emplace(std::forward<Args>(args)...);
    bind_return_storage.pop_back();
  }

  void inc_ref() {
    ++ref_count;
    std::cout << this << ": inc_ref -> " << ref_count << std::endl;
  }

  void maybe_destroy() {
    if (susp_count > 0 && ref_count == 0) {
      handle_type::from_promise(*this).destroy();
    }
  }

  void dec_ref() {
    --ref_count;
    std::cout << this << ": dec_ref -> " << ref_count << std::endl;
    maybe_destroy();
  }

  void on_suspend() {
    ++susp_count;
    std::cout << this << ": on_suspend -> " << susp_count << std::endl;
    maybe_destroy();
  }

  void on_resume() {
    --susp_count;
    std::cout << this << ": on_resume -> " << susp_count << std::endl;
  }

  auto get_return_object() { return make_return_object_holder(data); }
  auto initial_suspend() {
    bind_return_storage.push_back(&data->stage);
    return std::experimental::suspend_never{};
  }
  auto final_suspend() {
    std::cout << this << ": final_suspend" << std::endl;
    struct suspend : std::experimental::suspend_always {
      void await_suspend(handle_type h) { h.promise().on_suspend(); }
    };
    return suspend{};
  }

  template <typename U>
  auto await_transform(monad_rebind_t<M, U> e) {
    return monad_awaitable<decltype(e)>{std::move(e)};
  }

  // co_return with a value of the contained type is a shorthand for calling pure
  void return_value(typename monad_traits<M>::value_type x) {
    return_value(monad_traits<M>::pure(std::move(x)));
  }
  void return_value(M x) {
    std::cout << this << ": return_value called" << std::endl;
    emplace_value(std::move(x));
  }

  void unhandled_exception() {}
};

template <typename M>
struct monad_awaitable {
  M e;
  using T = typename monad_traits<M>::value_type;
  std::optional<T> value;

  constexpr bool await_ready() noexcept { return false; }

  constexpr auto await_resume() noexcept { return std::move(*value); }

  template <typename U>
  void await_suspend(
      std::experimental::coroutine_handle<monad_promise<monad_rebind_t<M, U>>> h) {
    // Register that we require the coroutine to stay alive so that we can write the
    // return value into it.
    auto sch = h.promise().sch;
    // Let the promise know that the coroutine is suspended.
    h.promise().on_suspend();

    // Create the continuation that we will pass to bind. This also registers that it
    // wants the coroutine to stay alive as long as the continuation stays alive so that
    // it can receive the return value of future suspend points.
    auto k = [this, h, sch](T x) mutable {
      // Set the value to be returned from co_await
      value = std::move(x);
      std::cout << this << ": calling resume on " << h.address() << std::endl;
      // Provide storage for the return value
      std::optional<monad_rebind_t<M, U>> storage;
      h.promise().push_storage(storage);
      // Return from co_await
      h.promise().on_resume();
      h.resume();
      std::cout << this << ": resume returned " << std::endl;
      // Return the result of the next bind or co_return
      if (!storage) {
        throw std::logic_error("Unpossible!");
      }
      std::cout << this << ": bind returning" << std::endl;
      return *storage;
    };
    // We bind our continuation into the value that was co_awaited. This returns, possibly
    // after resuming the coroutine, possibly immediately.
    std::cout << this << ": calling bind" << std::endl;
    auto tmp = e >>= std::move(k);
    std::cout << this << ": bind returned" << std::endl;
    h.promise().emplace_value(std::move(tmp));
  }
};

#endif  // MONAD_PROMISE_H
