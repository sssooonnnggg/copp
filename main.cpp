#include <coroutine>
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

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
struct task {
  ~task() {
    // Step 4: We should call coroutine_handle::destroy() to destroy coroutine
    taskHandle_.destroy();
    std::cout << "task destructor" << std::endl;
  }
  struct promise_type {
    promise_type() {
      std::cout
          << "task coroutine created: "
          << std::coroutine_handle<promise_type>::from_promise(*this).address()
          << std::endl;
    }
    ~promise_type() {
      std::cout
          << "task coroutine destroyed: "
          << std::coroutine_handle<promise_type>::from_promise(*this).address()
          << std::endl;
    }
    task get_return_object() noexcept {
      // Step 1: store coroutine handle associated with this task
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    // task will be executed deferred
    std::suspend_always initial_suspend() noexcept { return {}; }

    // Step 3: when task is finished, we should resume caller coroutine
    auto final_suspend() noexcept {
      // return final awaiter
      struct final_awaiter {
        // await_ready is used for optimisation, we always return false
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<>
        await_suspend(std::coroutine_handle<promise_type>) noexcept {
          // resume caller coroutine
          return callerHandle_;
        }
        void await_resume() noexcept {}
        std::coroutine_handle<> callerHandle_;
      };
      return final_awaiter{callerHandle_};
    }
    void return_void() noexcept {}
    // Currently we don't handle exceptions
    void unhandled_exception() noexcept {}
    // Hold caller coroutine's handle
    std::coroutine_handle<> callerHandle_;
  };

  // This awaiter will be called when executing co_await some task
  auto operator co_await() noexcept {
    struct awaiter {
      bool await_ready() noexcept { return false; }
      std::coroutine_handle<>
      await_suspend(std::coroutine_handle<> caller) noexcept {
        // Step 2:
        // Store caller coroutine's handle, caller's coroutine will be suspended
        taskHandle_.promise().callerHandle_ = caller;
        // Resume task's coroutine
        return taskHandle_;
      }
      void await_resume() noexcept {}
      std::coroutine_handle<promise_type> taskHandle_;
    };
    return awaiter{taskHandle_};
  }
  // Hold coroutine handle associative with this task
  std::coroutine_handle<promise_type> taskHandle_;
};

// Entry coroutine, used as coroutine's entry point
struct entry {
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

template <class T> struct function_traits;

template <class R, class... Args> struct function_traits<R(Args...)> {
  using result_type = R;
  using args_type = std::tuple<Args...>;
  using last_arg_type = std::tuple_element_t<sizeof...(Args) - 1, args_type>;
};

template <class R, class... Args>
struct function_traits<R (*)(Args...)> : public function_traits<R(Args...)> {};

template <class R, class... Args>
struct function_traits<std::function<R(Args...)>>
    : public function_traits<R(Args...)> {};

template <class T> struct tuple_pop_front;

template <class T, class... Args>
struct tuple_pop_front<std::tuple<T, Args...>> {
  using result_t = std::tuple<Args...>;
};

template <class T>
using tuple_pop_front_t = typename tuple_pop_front<T>::result_t;

static_assert(std::is_same_v<tuple_pop_front_t<std::tuple<int, float>>,
                             std::tuple<float>>);
static_assert(std::is_same_v<tuple_pop_front_t<std::tuple<int>>, std::tuple<>>);

template <class T, class U> struct tuple_pop_back_impl;

template <class T, std::size_t... I>
struct tuple_pop_back_impl<T, std::index_sequence<I...>> {
  using result_t = std::tuple<std::tuple_element_t<I, T>...>;
};

template <class T> struct tuple_pop_back;

template <class T, class... Args>
struct tuple_pop_back<std::tuple<T, Args...>> {
  constexpr static std::size_t N = sizeof...(Args);
  using tuple_type = std::tuple<T, Args...>;
  using helper_sequence = std::make_index_sequence<N>;
  using result_t =
      typename tuple_pop_back_impl<tuple_type, helper_sequence>::result_t;
};

template <class T> struct tuple_pop_back<std::tuple<T>> {
  using result_t = std::tuple<>;
};

template <class T>
using tuple_pop_back_t = typename tuple_pop_back<T>::result_t;

static_assert(
    std::is_same_v<tuple_pop_back_t<std::tuple<int, float>>, std::tuple<int>>);
static_assert(std::is_same_v<tuple_pop_back_t<std::tuple<int>>, std::tuple<>>);

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

////////////////////////////////////////////////////////////////////////////////////////////////////

//
// The test code is a simple asynchronous addition operation:
// There is an async function called async_add that takes two integers and a
// callback function as its last parameter. It returns the sum of the two
// integers via the callback. The result will be calculated in deferred ticks of
// the main loop.
//

struct async_add_operation {
  int a;
  int b;
  std::function<void(int)> callback;
};

struct simple_loop {
  std::vector<async_add_operation> operations_;
  void tick() {
    // execute all deferred operations in tick
    auto operations = std::move(operations_);
    for (auto &operation : operations) {
      operation.callback(operation.a + operation.b);
    }
  }
};

simple_loop main_loop;

void async_add(int a, int b, std::function<void(int)> callback) {
  main_loop.operations_.push_back({a, b, callback});
}

// Await async_add's result in a simple task
task simple_task(int a, int b) {
  //
  // Make a ordinary async function awaitable, e.g.
  // convert
  // `async_add(a, b, [](int result) { ... })`
  // to:
  // `auto [result] = awaitable_add(a, b)`
  //
  auto awaitable_add = make_awaitable(async_add);
  auto [result] = co_await awaitable_add(a, b);
  std::cout << "result: " << result << std::endl;
  co_return;
}

// Execute two simple tasks in entry coroutine in sequence
entry test_task() {
  co_await simple_task(1, 2);
  co_await simple_task(3, 4);
  co_return;
}

int main(int, char **) {
  auto entry = test_task();
  while (!entry.done()) {
    main_loop.tick();
  }
  return 0;
}
