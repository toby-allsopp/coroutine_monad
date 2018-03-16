#ifndef STATE_H
#define STATE_H

#include "monad_promise.h"

#include <functional>

/*
newtype State s a = State { runState :: (s -> (a,s)) }

instance Monad (State s) where
    return a        = State $ \s -> (a,s)
    (State x) >>= f = State $ \s -> let (v,s') = x s in runState (f v) s'
*/

template <typename S, typename A>
struct state {
  struct ret { A data; S state;};
  std::function<ret(S)> run;
};

template <typename S, typename A>
auto make_state_ret(A&& v, S&& s) {
  return typename state<std::decay_t<S>, std::decay_t<A>>::ret{std::forward<A>(v),
                                                               std::forward<S>(s)};
}

template <typename S, typename A>
struct monad_traits<state<S, A>> {
  using value_type = A;

  template <typename B>
  using rebind = state<S, B>;

  static auto pure(A a) {
    return state<S, A>{[=](S s) { return typename state<S, A>::ret{a, std::move(s)}; }};
  }

#define FWD(x) std::forward<decltype(x)>(x)

  template <typename F>
  static auto fmap(state<S, A> const& x, F&& f) {
    return rebind<decltype(f(std::declval<A>()))>{
        [x,f=std::forward<F>(f)](auto&& s) {
          auto [v, s2] = x.run(FWD(s));
          return make_state_ret(f(std::move(v)), std::move(s2));
        }};
  }

  template <typename ST, typename F>
  static auto bind(ST&& x, F&& f) {
    return state<S, typename monad_traits<decltype(f(std::declval<A>()))>::value_type>{
        [x=std::forward<ST>(x),f=std::forward<F>(f)](auto&& s) mutable {
          auto [v, s2] = x.run(FWD(s));
          return f(std::move(v)).run(std::move(s2));
        }};
  }
};

namespace std::experimental {
  // This makes state<S, A> useable as a coroutine return type.
  template <typename S, typename A, typename... Args>
  struct coroutine_traits<state<S, A>, Args...> {
    using promise_type = monad_promise<state<S, A>>;
  };
}

template <typename S>
static state<S, S> get = {[](auto&& s) { return make_state_ret(s, s); }};

using unit = std::tuple<>;

template <typename S>
state<std::decay_t<S>, unit> put(S&& s) {
  return {[s=std::forward<S>(s)](auto&&) { return make_state_ret(unit{}, std::move(s)); }};
}

#endif  // STATE_H
