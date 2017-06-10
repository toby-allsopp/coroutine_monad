#ifndef EXPECTED_H
#define EXPECTED_H

#include <type_traits>
#include <utility>

template <typename T, typename E>
class expected {
  union {
    T m_value;
    E m_error;
  };
  bool m_good;

 public:
  expected() : expected(T{}) {}
  expected(T value) : m_good(true) { m_value = std::move(value); }
  expected(E error) : m_good(false) { m_error = std::move(error); }
  ~expected() {
    if (m_good)
      m_value.~T();
    else
      m_error.~E();
  }

  expected(expected const& other) : m_good(other.m_good) {
    if (m_good)
      m_value = other.m_value;
    else
      m_error = other.m_error;
  }

  expected(expected&& other) : m_good(other.m_good) {
    if (m_good)
      m_value = std::move(other.m_value);
    else
      m_error = std::move(other.m_error);
  }

  expected& operator=(expected const& other) {
    this->~expected();
    new (this) expected(other);
    return *this;
  }

  void set_error(E error) {
    m_good  = false;
    m_error = std::move(error);
  }

  explicit operator bool() const { return m_good; }
  bool good() const { return m_good; }
  T const& value() const { return m_value; }
  T& value() { return m_value; }
  E const& error() const { return m_error; }
  E& error() { return m_error; }

  template <typename F>
  auto then(F&& f) const -> std::result_of_t<F(T)> {
    if (good()) return f(value());
    return error();
  }
  template <typename F>
  auto transform(F&& f) const -> expected<std::result_of_t<F(T)>, E> {
    if (good()) return f(value());
    return error();
  }
};

#endif  // EXPECTED_H
