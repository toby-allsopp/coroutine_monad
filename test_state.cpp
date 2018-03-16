#include "state.h"

#include "catch.hpp"

using random_state = int;
using my_state = state<random_state, double>;

auto random(random_state s) { return std::make_pair(static_cast<double>(s), s + 1); }

my_state getAny() {
  return get<random_state> >>= [](auto&& rs) {
    auto _ = random(rs);
    auto&& v = _.first;
    auto&& rs2 = _.second;
    return put(rs2) >>= [=](auto&&) { return pure<my_state>(v); };
  };
}

my_state getAny_co() {
  std::cout << "start\n";
  auto rs = co_await get<random_state>;
  std::cout << "rs = " << rs << "\n";
  auto[v, rs2] = random(rs);
  unit _ = co_await put(rs2);
  std::cout << "put returned\n";
  co_return v;
}

TEST_CASE("pure") {
  auto r = pure<my_state>(2.3).run(7);
  CHECK(r.data == 2.3);
  CHECK(r.state == 7);
}

TEST_CASE("get") {
  auto r = get<int>.run(7);
  CHECK(r.data == 7);
  CHECK(r.state == 7);
}

TEST_CASE("put") {
  auto r = put(42).run(7);
  CHECK(r.data == unit{});
  CHECK(r.state == 42);
}

TEST_CASE("fmap") {
  auto st = pure<my_state>(4.2);
  auto st2 = fmap(st, [](auto x) { return std::to_string(x); });
  auto r = st2.run(7);
  CHECK(r.data == std::to_string(4.2));
  CHECK(r.state == 7);
}

TEST_CASE("getAny") {
  auto r = getAny().run(7);
  CHECK(r.data == 7.0);
  CHECK(r.state == 8);
}

TEST_CASE("getAny_co") {
  auto st = getAny_co();
  auto r = st.run(7);
  CHECK(r.data == 7.0);
  CHECK(r.state == 8);
}

TEST_CASE("getAny_thrice") {
  auto st = getAny() >>= [](auto x) {
    return getAny() >>= [=](auto y) {
      return getAny() >>= [=](auto z) {
        return pure<monad_traits<my_state>::rebind<std::tuple<double, double, double>>>(
            std::make_tuple(x, y, z));
      };
    };
  };

  auto r = st.run(7);
  CHECK(r.data == std::make_tuple(7.0, 8.0, 9.0));
  CHECK(r.state == 10);
}

TEST_CASE("getAny_co_thrice") {
  auto st = []() -> monad_rebind_t<my_state, std::tuple<double, double, double>> {
    std::cout << "start\n";
    auto x = co_await getAny();
    std::cout << "resumed\n";
    auto y = co_await getAny();
    auto z = co_await getAny();
    co_return std::make_tuple(x, y, z);
  }();
  /*
   get<int> >>= [](auto x) { return
   getAny >>= [=](auto y) { return
   getAny >>= [=](auto z) { return
   std::make_tuple(x, y, z);
   }}};
   */
  auto r = st.run(7);
  CHECK(r.data == std::make_tuple(7.0, 8.0, 9.0));
  CHECK(r.state == 10);
}
