#pragma once

#include "errors.hpp"
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace gpagent::core {

// Result type for fallible operations
// Inspired by Rust's Result<T, E>
template<typename T, typename E = Error>
class Result {
public:
    // Constructors
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    Result(const E& error) : data_(error) {}
    Result(E&& error) : data_(std::move(error)) {}

    // Factory methods
    static Result<T, E> ok(T value) {
        return Result<T, E>(std::move(value));
    }

    static Result<T, E> err(E error) {
        return Result<T, E>(std::move(error));
    }

    static Result<T, E> err(ErrorCode code) {
        return Result<T, E>(E{code});
    }

    static Result<T, E> err(ErrorCode code, std::string message) {
        return Result<T, E>(E{code, std::move(message)});
    }

    static Result<T, E> err(ErrorCode code, std::string message, std::string context) {
        return Result<T, E>(E{code, std::move(message), std::move(context)});
    }

    // State checks
    bool is_ok() const { return std::holds_alternative<T>(data_); }
    bool is_err() const { return std::holds_alternative<E>(data_); }
    explicit operator bool() const { return is_ok(); }

    // Value accessors
    T& value() & {
        if (!is_ok()) {
            throw std::runtime_error("Result::value() called on error");
        }
        return std::get<T>(data_);
    }

    const T& value() const& {
        if (!is_ok()) {
            throw std::runtime_error("Result::value() called on error");
        }
        return std::get<T>(data_);
    }

    T&& value() && {
        if (!is_ok()) {
            throw std::runtime_error("Result::value() called on error");
        }
        return std::get<T>(std::move(data_));
    }

    // Error accessors
    E& error() & {
        if (!is_err()) {
            throw std::runtime_error("Result::error() called on ok");
        }
        return std::get<E>(data_);
    }

    const E& error() const& {
        if (!is_err()) {
            throw std::runtime_error("Result::error() called on ok");
        }
        return std::get<E>(data_);
    }

    E&& error() && {
        if (!is_err()) {
            throw std::runtime_error("Result::error() called on ok");
        }
        return std::get<E>(std::move(data_));
    }

    // Pointer-like access (returns nullptr if error)
    T* operator->() {
        return is_ok() ? &std::get<T>(data_) : nullptr;
    }

    const T* operator->() const {
        return is_ok() ? &std::get<T>(data_) : nullptr;
    }

    // Dereference (throws if error)
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(*this).value(); }

    // Map: Transform the value if ok, pass through error
    template<typename F>
    auto map(F&& f) const -> Result<std::invoke_result_t<F, const T&>, E> {
        using U = std::invoke_result_t<F, const T&>;
        if (is_ok()) {
            return Result<U, E>::ok(f(std::get<T>(data_)));
        }
        return Result<U, E>::err(std::get<E>(data_));
    }

    // and_then: Chain operations that return Result
    template<typename F>
    auto and_then(F&& f) const -> std::invoke_result_t<F, const T&> {
        using ResultType = std::invoke_result_t<F, const T&>;
        if (is_ok()) {
            return f(std::get<T>(data_));
        }
        return ResultType::err(std::get<E>(data_));
    }

    // or_else: Handle error case
    template<typename F>
    auto or_else(F&& f) const -> Result<T, E> {
        if (is_ok()) {
            return *this;
        }
        return f(std::get<E>(data_));
    }

    // map_err: Transform the error
    template<typename F>
    auto map_err(F&& f) const -> Result<T, std::invoke_result_t<F, const E&>> {
        using E2 = std::invoke_result_t<F, const E&>;
        if (is_ok()) {
            return Result<T, E2>::ok(std::get<T>(data_));
        }
        return Result<T, E2>::err(f(std::get<E>(data_)));
    }

    // unwrap_or: Get value or default
    T unwrap_or(T default_value) const {
        if (is_ok()) {
            return std::get<T>(data_);
        }
        return default_value;
    }

    // unwrap_or_else: Get value or compute from error
    template<typename F>
    T unwrap_or_else(F&& f) const {
        if (is_ok()) {
            return std::get<T>(data_);
        }
        return f(std::get<E>(data_));
    }

    // unwrap: Get value or throw
    T unwrap() const {
        if (is_ok()) {
            return std::get<T>(data_);
        }
        if constexpr (std::is_same_v<E, Error>) {
            throw std::runtime_error(std::get<E>(data_).full_message());
        } else {
            throw std::runtime_error("Result unwrap failed");
        }
    }

    // expect: Get value or throw with custom message
    T expect(const std::string& msg) const {
        if (is_ok()) {
            return std::get<T>(data_);
        }
        throw std::runtime_error(msg);
    }

private:
    std::variant<T, E> data_;
};

// Specialization for void value
template<typename E>
class Result<void, E> {
public:
    Result() : has_error_(false) {}
    Result(const E& error) : error_(error), has_error_(true) {}
    Result(E&& error) : error_(std::move(error)), has_error_(true) {}

    static Result<void, E> ok() {
        return Result<void, E>();
    }

    static Result<void, E> err(E error) {
        return Result<void, E>(std::move(error));
    }

    static Result<void, E> err(ErrorCode code) {
        return Result<void, E>(E{code});
    }

    static Result<void, E> err(ErrorCode code, std::string message) {
        return Result<void, E>(E{code, std::move(message)});
    }

    static Result<void, E> err(ErrorCode code, std::string message, std::string context) {
        return Result<void, E>(E{code, std::move(message), std::move(context)});
    }

    bool is_ok() const { return !has_error_; }
    bool is_err() const { return has_error_; }
    explicit operator bool() const { return is_ok(); }

    E& error() & {
        if (!has_error_) {
            throw std::runtime_error("Result::error() called on ok");
        }
        return error_;
    }

    const E& error() const& {
        if (!has_error_) {
            throw std::runtime_error("Result::error() called on ok");
        }
        return error_;
    }

    void unwrap() const {
        if (has_error_) {
            if constexpr (std::is_same_v<E, Error>) {
                throw std::runtime_error(error_.full_message());
            } else {
                throw std::runtime_error("Result unwrap failed");
            }
        }
    }

private:
    E error_;
    bool has_error_;
};

// Convenience alias
template<typename T>
using KResult = Result<T, Error>;

// Helper macros for early return on error
#define GPAGENT_TRY(expr) \
    ({ \
        auto&& _result = (expr); \
        if (_result.is_err()) { \
            return std::move(_result).error(); \
        } \
        std::move(_result).value(); \
    })

#define GPAGENT_TRY_VOID(expr) \
    do { \
        auto&& _result = (expr); \
        if (_result.is_err()) { \
            return _result; \
        } \
    } while (0)

}  // namespace gpagent::core
