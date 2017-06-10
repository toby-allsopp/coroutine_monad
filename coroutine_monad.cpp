#include "either.h"
#include "maybe.h"

#include "catch.hpp"

#include <iostream>

void print(int i) { std::cout << i << "\n"; }

template <template <typename> class Monad, typename T>
Monad<T> non_coroutine_pure(T&& x) {
  return {std::forward<T>(x)};
}

template <template <typename> class Monad, typename T>
Monad<T> non_coroutine_empty() {
  return {};
}

template <template <typename> class Monad>
Monad<int> bar(double x) {
  return int(x * 2);
}

TEST_CASE("coroutine lambda") {
  auto result = []() -> std::optional<int> {
    auto x = co_await non_coroutine_pure<std::optional>(1.5);
    auto y = co_await non_coroutine_pure<std::optional>(int(x * 2));
    co_return y;
  }();
  REQUIRE(result.value_or(42) == 3);
}

template <template <typename> class Monad>
Monad<int> doblock() {
  auto x = co_await non_coroutine_pure<Monad>(1.5);
  auto y = co_await non_coroutine_pure<Monad>(int(x * 2));
  co_return y;
}

TEST_CASE("coroutine optional") {
  auto result = doblock<std::optional>().value_or(42);
  REQUIRE(result == 3);
}

template <template <typename> class Monad>
Monad<int> doblock2() {
  auto x = co_await non_coroutine_empty<Monad, double>();
  auto y = co_await non_coroutine_pure<Monad>(int(x * 2));
  co_return y;
}

TEST_CASE("coroutine empty") {
  auto result = doblock2<std::optional>().value_or(42);
  REQUIRE(result == 42);
}

struct error {
  int code;
};

expected<int, error> f1() noexcept { return 7; }
expected<double, error> f2(int x) noexcept { return 2.0 * x; }
expected<int, error> f3(int x, double y) noexcept { return /*int(x + y)*/ error{42}; }

expected<int, error> test_expected_manual() {
  auto x = f1();
  if (!x) return x.error();
  auto y = f2(x.value());
  if (!y) return y.error();
  auto z = f3(x.value(), y.value());
  return z;
}

inline auto pair_with = [](auto&& x) {
  return [x](auto&& y) { return std::pair{std::move(x), std::forward<decltype(y)>(y)}; };
};

expected<int, error> test_expected_then() {
  auto z =
      f1().then([](auto x) { return f2(x).transform(pair_with(x)); }).then([](auto p) {
        auto[x, y] = p;
        return f3(x, y);
      });
  return z;
}

expected<int, error> test_expected_coroutine() {
  auto x = co_await f1();
  auto y = co_await f2(x);
  auto z = co_await f3(x, y);
  co_return z;
}

TEST_CASE("expected") {
  auto r = test_expected_coroutine();
  REQUIRE(!r.good());
  REQUIRE(r.error().code == 42);
}
