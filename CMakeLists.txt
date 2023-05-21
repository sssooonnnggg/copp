cmake_minimum_required(VERSION 3.0.0)
project(copp VERSION 0.1.0)

add_executable(copp_test tests/main.cpp)
target_compile_features(copp_test PUBLIC cxx_std_20)
target_include_directories(copp_test PUBLIC include)

