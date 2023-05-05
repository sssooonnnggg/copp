cmake_minimum_required(VERSION 3.0.0)
project(copp VERSION 0.1.0)

add_executable(copp main.cpp)

target_compile_features(copp PUBLIC cxx_std_20)

