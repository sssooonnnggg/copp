#pragma once

#include <coroutine>
#include <iostream>
#include <type_traits>

namespace copp {

//
// 1. Initialize the task and store the coroutine handle associated with the
// task.
// 2. When the caller calls co_await task, the caller's coroutine handle will be
// passed to the awaiter. We should store the caller's handle in the promise,
// suspend the caller, and then resume the task coroutine.
// 3. When the task is finished, we should resume the caller's coroutine.
// 4. When the task is destroyed (i.e., goes out of local variable lifetime), we
// should destroy the coroutine.
//
template <class D> struct promise_base {
  promise_base() {
    std::cout << "task coroutine created: "
              << std::coroutine_handle<D>::from_promise(static_cast<D &>(*this))
                     .address()
              << std::endl;
  }

  // task will be executed deferred
  std::suspend_always initial_suspend() noexcept { return {}; }

  // Step 3: when task is finished, we should resume caller coroutine
  auto final_suspend() noexcept {
    // return final awaiter
    struct final_awaiter {
      std::coroutine_handle<> callerHandle_;
      // await_ready is used for optimisation, we always return false
      bool await_ready() noexcept { return false; }
      std::coroutine_handle<> await_suspend(std::coroutine_handle<D>) noexcept {
        // resume caller coroutine
        return this->callerHandle_;
      }
      void await_resume() noexcept {}
    };
    return final_awaiter{callerHandle_};
  }
  // Currently we don't handle exceptions
  void unhandled_exception() noexcept {}
  // Hold caller coroutine's handle
  std::coroutine_handle<> callerHandle_;
};

template <class D, class T> struct promise_impl : public promise_base<D> {
  void return_value(T value) noexcept { this->value_ = value; }
  T value_;
};

template <class D> struct promise_impl<D, void> : public promise_base<D> {
  void return_void() noexcept {}
};

template <class T = void> struct task final {
  ~task() {
    // Step 4: We should call coroutine_handle::destroy() to destroy coroutine
    taskHandle_.destroy();
    std::cout << "task destructor" << std::endl;
  }
  struct promise_type : public promise_impl<promise_type, T> {
    task get_return_object() noexcept {
      // Step 1: store coroutine handle associated with this task
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
  };

  // This awaiter will be called when executing co_await some task
  auto operator co_await() noexcept {
    struct awaiter {
      bool await_ready() noexcept { return false; }
      std::coroutine_handle<>
      await_suspend(std::coroutine_handle<> caller) noexcept {
        // Step 2:
        // Store caller coroutine's handle, caller's coroutine will be suspended
        this->taskHandle_.promise().callerHandle_ = caller;
        // Resume task's coroutine
        return this->taskHandle_;
      }
      T await_resume() noexcept {
        if constexpr (std::is_same_v<T, void>) {
          return;
        } else {
          // Return task's result
          return this->taskHandle_.promise().value_;
        }
      }
      std::coroutine_handle<promise_type> taskHandle_;
    };
    return awaiter{taskHandle_};
  }
  // Hold coroutine handle associative with this task
  std::coroutine_handle<promise_type> taskHandle_;
};

} // namespace copp