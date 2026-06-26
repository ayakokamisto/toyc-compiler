#pragma once
/// Lightweight Result type for operations that may fail.
/// Uses std::expected when available (C++23), otherwise a simple variant.

#include <string>
#include <variant>

namespace toyc {

/// Result<T> — holds either a value of type T or an error string.
template <typename T>
class Result {
public:
  Result(T value) : data_(std::move(value)) {}               // NOLINT implicit
  Result(std::string error) : data_(std::move(error)) {}     // NOLINT implicit

  [[nodiscard]] bool ok() const { return std::holds_alternative<T>(data_); }
  [[nodiscard]] explicit operator bool() const { return ok(); }

  [[nodiscard]] const T& value() const { return std::get<T>(data_); }
  [[nodiscard]] T& value() { return std::get<T>(data_); }

  [[nodiscard]] const std::string& error() const { return std::get<std::string>(data_); }

private:
  std::variant<T, std::string> data_;
};

/// Specialization for void-returning operations.
template <>
class Result<void> {
public:
  Result() : error_() {}                                     // NOLINT implicit
  Result(std::string error) : error_(std::move(error)) {}    // NOLINT implicit

  [[nodiscard]] bool ok() const { return error_.empty(); }
  [[nodiscard]] explicit operator bool() const { return ok(); }
  [[nodiscard]] const std::string& error() const { return error_; }

private:
  std::string error_;
};

} // namespace toyc
