#pragma once

#include <functional>
#include <tuple>
#include <type_traits>

namespace copp {

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

} // namespace copp