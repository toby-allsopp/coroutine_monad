#ifndef RETURN_OBJECT_HOLDER_H
#define RETURN_OBJECT_HOLDER_H

#include <optional>
#include <utility>

template <typename T>
struct return_object_holder {
  // The staging object that is returned (by copy/move) to the caller of the coroutine.
  std::optional<T> stage;
  return_object_holder*& p;

  // When constructed, we assign a pointer to ourselves to the supplied reference to
  // pointer.
  return_object_holder(return_object_holder*& p) : stage{}, p(p) { p = this; }

  // Copying doesn't make any sense (which copy should the pointer refer to?).
  return_object_holder(return_object_holder const&) = delete;
  // To move, we just update the pointer to point at the new object.
  return_object_holder(return_object_holder&& other)
      : stage(std::move(other.stage)), p(other.p) {
    p = this;
  }

  // Assignment doesn't make sense.
  void operator=(return_object_holder const&) = delete;
  void operator=(return_object_holder&&) = delete;

  // A non-trivial destructor is required until
  // https://bugs.llvm.org//show_bug.cgi?id=28593 is fixed.
  ~return_object_holder() {}

  // Construct the staging value; arguments are perfect forwarded to T's constructor.
  template <typename... Args>
  void emplace(Args&&... args) {
    stage.emplace(std::forward<Args>(args)...);
  }

  // We assume that we will be converted only once, so we can move from the staging
  // object. We also assume that `emplace` has been called at least once.
  operator T() { return std::move(*stage); }
};

template <typename T>
auto make_return_object_holder(return_object_holder<T>*& p) {
  return return_object_holder<T>{p};
}

#endif  // RETURN_OBJECT_HOLDER_H
