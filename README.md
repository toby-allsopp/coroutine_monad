# coroutine_monad

This repository shows how coroutines can be used to compose operations that return types that either contain a result to be used in further computation or sn indication of error.

The goal is to make such composition as easy as Haskell's `do` notation.

## Requirements

The implemetation requires an implementation of the Coroutines TS (https://isocpp.org/files/papers/N4663.pdf). As of June 10, 2017, the only such implementation is a trunk build of clang with trunk libc++.

In particular, the MSVC implementation convert the object returned by `get_return_object` to the return type of the coroutine immediately rather than waiting until the coroutine returns to its caller.

## Example

See [`test_expected.cpp`](test_expected.cpp) for three different ways to compose a sequence of calls to functions returning `expected` values.
