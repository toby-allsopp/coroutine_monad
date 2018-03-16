#ifndef STATE_H
#define STATE_H

#include "monad_promise.h"

#include <functional>

/*!
= The State Monad

As an example of how to define the monad customization points, we implement the
State monad. In Haskell, one possible definition is as follows:

~~~haskell
newtype State s a = State { runState :: (s -> (a,s)) }

instance Functor (State s) where
    fmap f (State run) = State $ \s -> let (v,s') = run s in (f v, s')

instance Monad (State s) where
    return a          = State $ \s -> (a,s)
    (State run) >>= f = State $ \s -> let (v,s') = run s in runState (f v) s'
~~~

`State` is parameterized over two types: `s`, being the type of the State that
is threaded through the computation, and `a`, being the type of the result of
the computation.

A value of type `State` is just a function from a value of the State type to a
pair of the result value and the new State value.

In addition, it is traditional to define some helper combinators to make working
with stateful computations easier:

~~~haskell
get :: State s s
get = State $ \s -> (s, s)

put :: a -> State a ()
put x = State $ \_ -> ((), x)
~~~

*/

#define FWD(x) std::forward<decltype(x)>(x)

namespace toby::state {
  template <typename A, typename S>
  struct RunResult {
    A data;
    S state;
  };

  // clang-format off
  template <typename A, typename S>
  RunResult(A, S) -> RunResult<A, S>;
  // clang-format on

  template <typename F>
  struct RawState {
    F run;

    // operator version of non-type-erased bind
    template <typename M, typename FF>
    friend constexpr auto operator>>=(M&& m, FF&& f) {
      return bind(std::forward<M>(m), std::forward<FF>(f));
    }
  };

  // clang-format off
  template <typename F>
  RawState(F) -> RawState<F>;
  // clang-format on

  // Non-type-erased pure
  template <typename A>
  constexpr auto pure(A&& x) {
    return RawState{[x = std::forward<A>(x)](auto&& s) {
      return RunResult{x, FWD(s)};
    }};
  }

  // Non-type-erased transform
  template <typename M, typename F>
  constexpr auto transform(M&& x, F&& f) {
    return RawState{[x = std::forward<M>(x), f = std::forward<F>(f)](auto&& s) {
      auto ret = x.run(FWD(s));
      return RunResult{f(std::move(ret.data)), std::move(ret.state)};
    }};
  }

  // Non-type-erased bind
  template <typename M, typename F>
  constexpr auto bind(M&& x, F&& f) {
    return RawState{[x = std::forward<M>(x), f = std::forward<F>(f)](auto&& s) {
      auto ret = x.run(FWD(s));
      return f(std::move(ret.data)).run(std::move(ret.state));
    }};
  }

  using unit = std::tuple<>;

  constexpr inline auto get = RawState{[](auto&& s) {
    return RunResult{s, FWD(s)};
  }};

  template <typename S>
  constexpr auto put(S&& s) {
    return RawState{[s = std::forward<S>(s)](auto&&) {
      return RunResult{unit{}, s};
    }};
  }

  // Type-erased version that can fit into the type-constructor system.

  template <typename FTC, typename S, typename A>
  struct State {
    using value_type = A;

    std::experimental::meta::invoke<FTC, RunResult<A, S>, S> run;

    template <typename F>
    State(RawState<F> rs) : run(std::move(rs.run)) {}
  };

  template <typename FTC, typename S>
  struct StateTC {
    template <typename... A>
    using invoke = State<FTC, S, A...>;

    template <typename... A>
    using t = invoke<A...>;

    template <typename A>
    static auto pure(A&& x) -> t<std::experimental::meta::uncvref_t<A>> {
      return toby::state::pure(std::forward<A>(x));
    }

    static inline t<S> const get = toby::state::get;

    template <typename SS>
    static auto put(SS&& s) -> t<unit> {
      return toby::state::put(std::forward<SS>(s));
    }
  };
}  // namespace toby::state

#undef FWD

namespace std::experimental {
  template <typename FTC, typename S, typename A>
  struct type_constructor<toby::state::State<FTC, S, A>>
      : meta::id<toby::state::StateTC<FTC, S>> {};

  namespace type_constructible {
    template <typename FTC, typename S, typename A>
    struct traits<toby::state::State<FTC, S, A>> {
      template <typename M, typename X>
      static auto make(X&& x) {
        return toby::state::StateTC<FTC, S>::pure(std::forward<X>(x));
      }
    };
  }  // namespace type_constructible

  namespace functor {
    template <typename FTC, typename S>
    struct traits<toby::state::StateTC<FTC, S>> : mcd_transform {
      template <typename M, typename F>
      static auto transform(M&& x, F&& f) -> toby::state::
          State<FTC, S, invoke_result_t<F, value_type_t<meta::uncvref_t<M>>>> {
        return toby::state::transform(std::forward<M>(x), std::forward<F>(f));
      }
    };
  }  // namespace functor

  namespace monad {
    template <typename FTC, typename S>
    struct traits<toby::state::StateTC<FTC, S>> : mcd_bind {
      template <typename M, typename F>
      static auto bind(M&& x, F&& f) -> toby::state::State<
          FTC,
          S,
          value_type_t<invoke_result_t<F, value_type_t<meta::uncvref_t<M>>>>> {
        return toby::state::bind(std::forward<M>(x), std::forward<F>(f));
      }
    };
  }  // namespace monad
}  // namespace std::experimental

namespace std::experimental {
  // This makes State<FTC, S, A> useable as a coroutine return type.
  template <typename FTC, typename S, typename A, typename... Args>
  struct coroutine_traits<toby::state::State<FTC, S, A>, Args...> {
    using promise_type = monad_promise<toby::state::State<FTC, S, A>>;
  };
}  // namespace std::experimental

#endif  // STATE_H
