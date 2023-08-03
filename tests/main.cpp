#include <coroutine>
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

#include "awaitable.h"
#include "entry.h"
#include "task.h"

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
copp::task<int> simple_task(int a, int b) {
  //
  // Make a ordinary async function awaitable, e.g.
  // convert
  // `async_add(a, b, [](int result) { ... })`
  // to:
  // `auto [result] = awaitable_add(a, b)`
  //
  auto awaitable_add = copp::make_awaitable(async_add);
  auto [result] = co_await awaitable_add(a, b);
  std::cout << "result: " << result << std::endl;
  co_return result;
  //   co_return;
}

// Execute two simple tasks in entry coroutine in sequence
copp::entry test_task() {
  auto result1 = co_await simple_task(1, 2);
  std::cout << "result from co_await: " << result1 << std::endl;
  //   co_await simple_task(3, 4);
  co_return;
}

int main(int, char **) {
  auto entry = test_task();
  while (!entry.done()) {
    main_loop.tick();
  }
  return 0;
}
