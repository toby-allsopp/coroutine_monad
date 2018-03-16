# Using coroutines for monadic composition

This repository shows how coroutines can be used to compose operations that
return monadic types.

One example of a monadic type is one that either contain a result to be used in
further computation or an indication of error, such as `std::optional<T>`.

The goal is to make such composition as easy as Haskell's `do` notation.

## Requirements

The implementation requires an implementation of the Coroutines TS
(https://isocpp.org/files/papers/N4680.pdf). As of March 10, 2018, the only such
implementation is Clang 5 or later with a corresponding libc++.

In particular, the MSVC implementation converts the object returned by
`get_return_object` to the return type of the coroutine immediately rather than
waiting until the coroutine returns to its caller.

## Expected

See [`test_expected.cpp`](test_expected.cpp) for three different ways to compose
a sequence of calls to functions returning `expected` values.

The `expected` in question is that from viboes' std-make repository. This
definition knows nothing about coroutines; all of the coroutine machinery is in
[`moand_promise.h`](moand_promise.h) and
[`test_expected.cpp`](test_expected.cpp).

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

expected<int, error> f1() { return 7; }
expected<double, error> f2(int x) { return 2.0 * x; }
expected<int, error> f3(int x, double y) { return error{42}; }

auto test_expected_coroutine() {
  return []() -> expected<int, error> {
    auto x = co_await f1();
    auto y = co_await f2(x);
    auto z = co_await f3(x, y);
    co_return z;
  }();
}
```

## State

An implementation of the State monad can be found in [`state.h`](state.h).
Examples of its usage, both with and without coroutines, are in
[`test_state.cpp`](test_state.cpp).
