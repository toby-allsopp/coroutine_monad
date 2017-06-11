#include "either.h"

#include "catch.hpp"

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
      .then([](auto x) { return f2(x).transform(pair_with(x)); })
      .then([](auto p) { auto[x, y] = p; return f3(x, y); });
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

TEST_CASE("expected") {
  auto r = test_expected_coroutine();
  REQUIRE(!r.good());
  REQUIRE(r.error().code == 42);
}
