#pragma once

#include <coroutine>
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

#include "utils.h"

template <class F, class U> struct awaiter;

//
// An awaiter to wrap a normal async function with a callback as its last
// parameter
//
template <class F, class... Args> struct awaiter<F, std::tuple<Args...>> {
  F func_;
  std::tuple<Args...> args_;
  using callback_type = typename function_traits<F>::last_arg_type;
  using result_type = typename function_traits<callback_type>::args_type;
  result_type result_;
  void await_suspend(std::coroutine_handle<> handle) noexcept {
    auto full_args = std::tuple_cat(
        std::move(args_), std::make_tuple([handle, this](auto &&...args) {
          result_ = std::make_tuple(std::forward<decltype(args)>(args)...);
          handle.resume();
        }));
    std::apply(func_, full_args);
  }
  bool await_ready() noexcept { return false; }
  result_type await_resume() noexcept { return result_; }
};

template <class T> struct make_awaitable_helper;

template <class... Args> struct make_awaitable_helper<std::tuple<Args...>> {
  template <class F> static auto make_awaitable(F func) {
    return [func](Args... args) {
      return awaiter<F, std::tuple<std::decay_t<Args>...>>{
          func, std::make_tuple(std::forward<Args>(args)...)};
    };
  }
};

// This is a helper function that allows an async function to be made awaitable,
// similar to the functionality of utils.promisify in Node.js.
template <class F> auto make_awaitable(F func) {
  using args_t = typename function_traits<F>::args_type;
  using args_without_callback_t = tuple_pop_back_t<args_t>;
  return make_awaitable_helper<args_without_callback_t>::make_awaitable(func);
}