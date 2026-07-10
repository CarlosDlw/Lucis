cmake_minimum_required(VERSION 3.20)
project(lucis-fuzzer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(lucis-fuzzer
    main.cpp
    corpus.cpp
    mutator.cpp
    runner.cpp
    result.cpp
)

target_compile_options(lucis-fuzzer PRIVATE -Wall -Wextra -O2)
target_link_libraries(lucis-fuzzer PRIVATE Threads::Threads)
