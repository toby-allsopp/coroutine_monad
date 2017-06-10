#ifndef RETURN_OBJECT_HOLDER_H
#define RETURN_OBJECT_HOLDER_H

#include <utility>

template <typename T>
struct return_object_holder {
  // The staging object that is returned (by copy/move) to the caller of the coroutine.
  T stage;

  // When constructed, we construct the staging object by forwarding the args and then
  // assign a pointer to it to the supplied reference to pointer.
  template <typename... Args>
  return_object_holder(T*& p, Args&&... args) : stage{std::forward<Args>(args)...} {
    p = &stage;
  }

  // Because we rely on the address of the stage member remaining constant, we forbid
  // copying and assignment.
  return_object_holder(return_object_holder const&) = delete;
  return_object_holder(return_object_holder&&)      = delete;
  void operator=(return_object_holder const&) = delete;
  void operator=(return_object_holder&&) = delete;

  // A non-trivial destructor is required until
  // https://bugs.llvm.org//show_bug.cgi?id=28593 is fixed.
  ~return_object_holder() {}

  // We assume that we will be converted only once, so we can move from the staging
  // object.
  operator T() { return std::move(stage); }
};

template <typename T>
auto make_return_object_holder(T*& p) {
  return return_object_holder<T>{p};
}

#endif  // RETURN_OBJECT_HOLDER_H
