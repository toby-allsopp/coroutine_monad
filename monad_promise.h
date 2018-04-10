#ifndef MONAD_PROMISE_H
#define MONAD_PROMISE_H

#include "return_object_holder.h"

#include <experimental/functor.hpp>
#include <experimental/fundamental/v3/value_type.hpp>
#include <experimental/make.hpp>
#include <experimental/monad.hpp>

#include <experimental/coroutine>
#include <iostream>
#include <tuple>
#include <vector>

template <typename M, typename F>
auto operator>>=(M&& m, F&& f)
    -> decltype(std::experimental::monad::bind(std::forward<M>(m),
                                               std::forward<F>(f))) {
  return std::experimental::monad::bind(std::forward<M>(m), std::forward<F>(f));
}

template <typename P>
struct shared_coroutine_handle {
  using handle_type = std::experimental::coroutine_handle<P>;
  handle_type h = {};

  shared_coroutine_handle() = default;
  shared_coroutine_handle(handle_type h) : h(h) {
    // Note that we don't increment the reference count stored in h.promise()
    // here. This is because of the way the promise itself holds a
    // shared_coroutine_handle to itself.
  }
  shared_coroutine_handle(shared_coroutine_handle const& o)
      : shared_coroutine_handle() {
    *this = o;
    std::cout << this << ": shared_coroutine_handle copy constructor: h = "
              << h.address() << std::endl;
  }
  shared_coroutine_handle(shared_coroutine_handle&& o) noexcept {
    h = std::exchange(o.h, {});
    std::cout << this << ": shared_coroutine_handle move constructor: h = "
              << h.address() << std::endl;
  }

  shared_coroutine_handle& operator=(shared_coroutine_handle const& o) {
    reset();
    h = o.h;
    if (h) h.promise().inc_ref();
    return *this;
  }
  shared_coroutine_handle& operator=(shared_coroutine_handle&& o) {
    reset();
    h = std::exchange(o.h, {});
    return *this;
  }

  ~shared_coroutine_handle() {
    std::cout << this << ": ~shared_coroutine_handle: h = " << h.address()
              << std::endl;
    reset();
  }

  void reset() {
    auto h2 = std::exchange(h, {});
    if (h2) h2.promise().dec_ref();
  }
};

template <typename M>
struct monad_awaitable;

template <typename M>
struct monad_promise {
  using handle_type = std::experimental::coroutine_handle<monad_promise>;

  // So that we can defer initialization of the return object until we know
  // what the return value of the coroutine will be.
  return_object_holder<M>* return_object;

  // A stack of places to store the results of the monadic bind operations.
  // These pointers point either into the return object in the coroutine frame
  // (the bottom of the stack) or to local variables in the continuations passed
  // to bind (the rest of the stack).
  std::vector<deferred<M>*> bind_return_storage;

  int ref_count = 0;
  // The use of unique_ptr here is because MSVC 14.11 can't instantiate
  // coroutine_handle for an incomplete type.
  std::unique_ptr<shared_coroutine_handle<monad_promise>> psch =
      std::make_unique<shared_coroutine_handle<monad_promise>>(
          handle_type::from_promise(*this));
  shared_coroutine_handle<monad_promise>& sch = *psch;
  int susp_count = 0;

  ~monad_promise() { std::cout << this << ": ~monad_promise" << std::endl; }

  void push_storage(deferred<M>& storage) {
    bind_return_storage.push_back(&storage);
  }

  template <typename... Args>
  void emplace_value(Args&&... args) {
    auto storage = bind_return_storage.back();
    if (storage != &return_object->stage) {
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

  void dec_ref() {
    --ref_count;
    std::cout << this << ": dec_ref -> " << ref_count << std::endl;
    maybe_destroy();
  }

  void on_suspend() {
    ++susp_count;
    // susp_count should always be == 1 here
    std::cout << this << ": on_suspend -> " << susp_count << std::endl;
    maybe_destroy();
  }

  void on_resume() {
    --susp_count;
    // susp_count should always be == 0 here
    std::cout << this << ": on_resume -> " << susp_count << std::endl;
  }

  // We destroy the coroutine if it is suspended and unreferenced. If it is not
  // suspended then it will flow off the end and be destroyed automatically. If
  // it is unreferenced then we know it will never be resumed, so needs to be
  // destroyed,
  void maybe_destroy() {
    if (susp_count > 0 && ref_count == 0) {
      handle_type::from_promise(*this).destroy();
    }
  }

  auto get_return_object() { return make_return_object_holder(return_object); }

  auto initial_suspend() {
    // N4680 says that get_return_object is called before initial_suspend, but
    // MSVC 14.1 calls initial_suspend first. However, it does call
    // get_return_object before calling await_ready on the result of
    // initial_suspend().
    struct suspend : std::experimental::suspend_never {
      monad_promise* p;
      suspend(monad_promise* p) : p(p) {}
      bool await_ready() {
        // The first item on the stack of places to store the results of bind
        // is the return value of the coroutine itself.
        // We rely on get_return_object having been called already as required
        // by N4680.
        p->bind_return_storage.push_back(&p->return_object->stage);
        return true;
      }
    };
    return suspend(this);
  }
  auto final_suspend() {
    std::cout << this << ": final_suspend" << std::endl;
    struct suspend : std::experimental::suspend_always {
      void await_suspend(handle_type h) { h.promise().on_suspend(); }
    };
    return suspend{};
  }

  using TC = std::experimental::type_constructor_t<M>;
  using ValueType = std::experimental::value_type_t<M>;

  // co_await is allowed for any type N such that we can construct a value of
  // our monad with N
  template <typename N,
            typename U = std::experimental::value_type_t<
                std::experimental::meta::uncvref_t<N>>,
            typename O = std::experimental::meta::invoke<TC, U>,
            typename = std::enable_if_t<std::is_constructible_v<O, N>>>
  auto await_transform(N&& m) {
    return monad_awaitable<O>{std::forward<N>(m)};
  }

  template <typename T>
  void return_value(T&& x) {
    if constexpr (std::is_same_v<std::experimental::meta::uncvref_t<T>,
                                 ValueType>) {
      // co_return with a value of the contained type is a shorthand for calling
      // pure
      return_value(std::experimental::make<TC>(std::forward<T>(x)));
    } else {
      std::cout << this << ": return_value called" << std::endl;
      emplace_value(std::forward<T>(x));
    }
  }

  void unhandled_exception() {}
};

template <typename M>
struct monad_awaitable {
  M x;
  using T = std::experimental::value_type_t<M>;
  deferred<T> result;

  monad_awaitable(M x) : x(std::move(x)) {
    std::cout << this << ": monad_awaitable()" << std::endl;
  }

  ~monad_awaitable() {
    std::cout << this << ": ~monad_awaitable()" << std::endl;
  }

  constexpr bool await_ready() noexcept { return false; }

  constexpr auto await_resume() noexcept {
    std::cout << this << ": await_resume" << std::endl;
    return std::move(*result);
  }

  template <typename N>
  struct continuation {
    monad_awaitable& awaitable;
    std::experimental::coroutine_handle<monad_promise<N>> h;
    shared_coroutine_handle<monad_promise<N>> sch;
    std::shared_ptr<bool> invoked = std::make_shared<bool>(false);

    continuation(continuation const&) = delete;
    continuation(continuation&&) = default;

    continuation& operator=(continuation const&) = delete;
    continuation& operator=(continuation &&) = delete;

    template <typename T>
    auto operator()(T&& x) && {
      std::cout << &awaitable << ": continuation invoked" << std::endl;
      if (!sch.h)
        throw std::logic_error(
            "coroutine continuation invoked after being moved from");
      if (std::exchange(*invoked, true))
        throw std::logic_error("coroutine continuation invoked more than once");
      auto local_sch = std::move(sch);
      // Set the value to be returned from co_await
      awaitable.result.emplace(std::forward<decltype(x)>(x));
      std::cout << this << ": calling resume on " << h.address() << std::endl;
      // Provide storage for the return value
      deferred<N> storage;
      h.promise().push_storage(storage);
      // Let the promise know that the coroutine is (about to be) resumed.
      h.promise().on_resume();
      // Resume the coroutine, returning from co_await
      h.resume();
      std::cout << this << ": resume returned " << std::endl;
      // Return the result of the next bind or co_return
      std::cout << this << ": bind returning" << std::endl;
      return *storage;
    }
  };

  template <typename N>
  void await_suspend(std::experimental::coroutine_handle<monad_promise<N>> h) {
    // Register that we require the coroutine to stay alive so that we can write
    // the return value into it.
    auto sch = h.promise().sch;
    // Let the promise know that the coroutine is suspended.
    h.promise().on_suspend();

    // Create the continuation that we will pass to bind. This also registers
    // that it wants the coroutine to stay alive as long as the continuation
    // stays alive so that it can receive the return value of future suspend
    // points.
    auto k = continuation<N>{*this, h, sch};
    // We call bind with the value that was co_awaited and our continuation. The
    // implementation of bind can choose to call the continuation before
    // returning or some time later or never.
    std::cout << this << ": calling bind" << std::endl;
    auto tmp = std::experimental::monad::bind(std::move(x), std::move(k));
    std::cout << this << ": bind returned" << std::endl;
    h.promise().emplace_value(std::move(tmp));
  }
};

#endif  // MONAD_PROMISE_H
