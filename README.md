# Using coroutines for monadic composition

This repository shows how coroutines can be used to compose operations that return types that either contain a result to be used in further computation or sn indication of error.

The goal is to make such composition as easy as Haskell's `do` notation.

## Requirements

The implemetation requires an implementation of the Coroutines TS (https://isocpp.org/files/papers/N4663.pdf). As of June 10, 2017, the only such implementation is a trunk build of clang with trunk libc++.

In particular, the MSVC implementation convert the object returned by `get_return_object` to the return type of the coroutine immediately rather than waiting until the coroutine returns to its caller.

## Example

See [`test_expected.cpp`](test_expected.cpp) for three different ways to compose a sequence of calls to functions returning `expected` values.

The `expected` in question is a very simple example defined in [`expected.h`](expected.h); this is not intended to be anywhere near fully-featured (and is probably not even correct). This definition knows nothing about coroutines; all of the coroutine machinery is in [`either.h`](either.h).

Here's what one can write in Haskell:

```haskell
data MyError = MyError Int

f1 :: Either MyError Int
f1 = Right 7

f2 :: Int -> Either MyError Float
f2 x = Right (fromIntegral x * 2.0)

f3 :: Int -> Float -> Either MyError Int
f3 x y = Left (MyError 42)

test :: Either MyError Int
test =
  do x <- f1
     y <- f2 x
     z <- f3 x y
     return z
```

With the code in the repository, you can write it like this in C++:

```c++
struct error {
  int code;
};

expected<int, error> f1() noexcept { return 7; }
expected<double, error> f2(int x) noexcept { return 2.0 * x; }
expected<int, error> f3(int x, double y) noexcept { return error{42}; }

auto test_expected_coroutine() {
  return []() -> expected<int, error> {
    auto x = co_await f1();
    auto y = co_await f2(x);
    auto z = co_await f3(x, y);
    co_return z;
  }();
}
```
