#include "state.h"

#include "catch.hpp"

namespace stde = std::experimental;

struct random_state {
  int next_value;

  friend bool operator==(random_state lhs, random_state rhs) {
    return lhs.next_value == rhs.next_value;
  }
};

struct StdFunctionTC {
  template <typename R, typename... A>
  using invoke = std::function<R(A...)>;
};

namespace std::experimental::type_constructible {
  template <>
  struct traits<StdFunctionTC> {
    template <typename R, typename... A, typename F>
    static auto make(F&& f) {
      return std::function<R(A...)>(std::forward<F>(f));
    }
  };
}  // namespace std::experimental::type_constructible

using MyState = toby::state::StateTC<StdFunctionTC, random_state>;

auto random(random_state s) -> std::pair<double, random_state> {
  return {static_cast<double>(s.next_value), {s.next_value + 1}};
}

MyState::t<double> const next_random = MyState::get >>= [](auto&& rs) {
  auto _ = random(rs);
  auto&& v = _.first;
  auto&& rs2 = _.second;
  return MyState::put(rs2) >>= [=](auto&&) { return MyState::pure(v); };
};

auto const next_random_co = []() -> MyState::t<double> {
  auto rs = co_await MyState::get;
  auto [v, rs2] = random(rs);
  auto _ = co_await MyState::put(rs2);
  co_return v;
};

TEST_CASE("pure raw") {
  auto r = toby::state::pure(2.3).run(random_state{7});
  CHECK(r.data == 2.3);
  CHECK(r.state == random_state{7});
}

TEST_CASE("pure type-erased") {
  auto r = MyState::pure(2.3).run({7});
  CHECK(r.data == 2.3);
  CHECK(r.state == random_state{7});
}

TEST_CASE("get raw") {
  CHECK(std::is_const_v<decltype(toby::state::get)>);
  auto r = toby::state::get.run(random_state{7});
  CHECK(r.data == random_state{7});
  CHECK(r.state == random_state{7});
}

TEST_CASE("get type-erased") {
  auto r = MyState::get.run({7});
  CHECK(r.data == random_state{7});
  CHECK(r.state == random_state{7});
}

TEST_CASE("put raw") {
  auto r = toby::state::put(random_state{42}).run(random_state{7});
  CHECK(r.data == toby::state::unit{});
  CHECK(r.state == random_state{42});
}

TEST_CASE("put type-erased") {
  auto r = MyState::put(random_state{42}).run({7});
  CHECK(r.data == toby::state::unit{});
  CHECK(r.state == random_state{42});
}

TEST_CASE("fmap raw") {
  auto st = toby::state::pure(4.2);
  auto st2 =
      toby::state::transform(st, [](auto x) { return std::to_string(x); });
  auto r = st2.run(7);
  CHECK(r.data == std::to_string(4.2));
  CHECK(r.state == 7);
}

TEST_CASE("fmap type-erased") {
  auto st = toby::state::StateTC<StdFunctionTC, int>::pure(4.2);
  auto st2 =
      stde::functor::transform(st, [](auto x) { return std::to_string(x); });
  auto r = st2.run(7);
  CHECK(r.data == std::to_string(4.2));
  CHECK(r.state == 7);
}

TEST_CASE("next_random") {
  auto r = next_random.run({7});
  CHECK(r.data == 7.0);
  CHECK(r.state == random_state{8});
}

TEST_CASE("next_random_co") {
  auto const st = next_random_co();
  auto r = st.run({7});
  CHECK(r.data == 7.0);
  CHECK(r.state == random_state{8});
  // r = st.run({7}); // NOPE - coroutine-based state values are one-shot!
  auto const st2 = next_random_co();
  r = st2.run({8});
  CHECK(r.data == 8.0);
  CHECK(r.state == random_state{9});
}

TEST_CASE("next_random_thrice") {
  auto st = next_random >>= [](auto x) {
    return next_random >>= [=](auto y) {
      return next_random >>= [=](auto z) {
        return MyState::pure(std::tuple{x, y, z});
      };
    };
  };

  auto r = st.run({7});
  CHECK(r.data == std::make_tuple(7.0, 8.0, 9.0));
  CHECK(r.state == random_state{10});
}

TEST_CASE("next_random_co_thrice") {
  auto st = []() -> MyState::t<std::tuple<double, double, double>> {
    auto x = co_await next_random_co();
    auto y = co_await next_random_co();
    auto z = co_await next_random_co();
    co_return std::make_tuple(x, y, z);
  }();
  auto r = st.run({7});
  CHECK(r.data == std::make_tuple(7.0, 8.0, 9.0));
  CHECK(r.state == random_state{10});
}
