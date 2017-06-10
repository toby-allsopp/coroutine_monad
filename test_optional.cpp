#include "maybe.h"

#include "catch.hpp"

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
