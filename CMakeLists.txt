cmake_minimum_required(VERSION 3.15)
project(nanomatch CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT MSVC)
  add_compile_options(-O3 -march=native -fno-exceptions)
endif()

add_executable(nanomatch
  src/main.cpp
  src/order_book.cpp
  src/baseline_book.cpp
  src/parser.cpp
)
