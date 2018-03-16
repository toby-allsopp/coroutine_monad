#include <experimental/expected.hpp>

#include "monad_promise.h"

#include "catch.hpp"

// This makes expected<T, E> useable as a coroutine return type.
namespace std::experimental {
  template <typename T, typename E, typename... Args>
  struct coroutine_traits<expected<T, E>, Args...> {
    using promise_type = monad_promise<expected<T, E>>;
  };
}  // namespace std::experimental

using std::experimental::expected;
using std::experimental::make_unexpected;

struct error {
  int code;
};

expected<int, error> f1() noexcept { return 7; }
expected<double, error> f2(int x) noexcept { return 2.0 * x; }
expected<int, error> f3(int x, double y) noexcept {
  return /*int(x + y)*/ make_unexpected(error{42});
}

expected<int, error> test_expected_manual() {
  auto x = f1();
  if (!x) return make_unexpected(x.error());
  auto y = f2(*x);
  if (!y) return make_unexpected(y.error());
  auto z = f3(*x, *y);
  return z;
}

template <typename T>
auto pair_with(T&& x) {
  return [x](auto&& y) {
    return std::make_pair(std::move(x), std::forward<decltype(y)>(y));
  };
}

expected<int, error> test_expected_then() {
  // clang-format off
  auto z =
      f1()
      .bind([](int x) { return f2(x).map(pair_with(x)); })
      .bind([](auto p) { auto[x, y] = p; return f3(x, y); });
  // clang-format on
  return z;
}

auto test_expected_coroutine() {
  return []() -> expected<int, error> {
    auto x = co_await f1();
    auto y = co_await f2(x);
    auto z = co_await f3(x, y);
    co_return z;
  }();
}

TEST_CASE("manual") {
  auto r = test_expected_manual();
  REQUIRE(!r.valid());
  REQUIRE(r.error().code == 42);
}

TEST_CASE("then") {
  auto r = test_expected_then();
  REQUIRE(!r.valid());
  REQUIRE(r.error().code == 42);
}

TEST_CASE("expected") {
  auto r = test_expected_coroutine();
  REQUIRE(!r.valid());
  REQUIRE(r.error().code == 42);
}

TEST_CASE("return") {
  auto r = []() -> expected<int, error> { co_return 7; }();
  REQUIRE(r.valid());
  REQUIRE(r.value() == 7);
}

TEST_CASE("await good") {
  auto r = []() -> expected<int, error> {
    auto x = co_await expected<int, error>(7);
    REQUIRE(x == 7);
    co_return x;
  }();
  REQUIRE(r.valid());
  REQUIRE(r.value() == 7);
}

TEST_CASE("await error") {
  auto r = []() -> expected<int, error> {
    auto x = co_await expected<int, error>(make_unexpected(error{42}));
    FAIL();
    co_return x;
  }();
  REQUIRE(!r.valid());
  REQUIRE(r.error().code == 42);
}
