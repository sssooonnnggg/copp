#pragma once

#include <coroutine>
#include <iostream>
#include <type_traits>

namespace copp {

// Entry coroutine, used as coroutine's entry point
struct entry final {
  struct promise_type {
    promise_type() {
      std::cout
          << "entry coroutine created: "
          << std::coroutine_handle<promise_type>::from_promise(*this).address()
          << std::endl;
    }
    ~promise_type() {
      std::cout
          << "entry coroutine destroyed: "
          << std::coroutine_handle<promise_type>::from_promise(*this).address()
          << std::endl;
    }
    entry get_return_object() noexcept {
      return entry{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    // Entry will not suspend when started
    std::suspend_never initial_suspend() noexcept { return {}; }
    // Entry will be destroyed in entry's destructor
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
  };
  ~entry() { handle_.destroy(); }
  bool done() const { return handle_.done(); }
  std::coroutine_handle<promise_type> handle_;
};

} // namespace copp