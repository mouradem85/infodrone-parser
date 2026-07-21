cmake_minimum_required(VERSION 3.20)
project(InfodroneParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic)
endif()

include_directories(include)

add_executable(infodrone_parser
    src/main.cpp
    src/PcapDataSource.cpp
)