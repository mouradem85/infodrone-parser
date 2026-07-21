cmake_minimum_required(VERSION 3.20)
project(InfodroneParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options d'optimisation pour la performance / bas niveau
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic)
endif()

# Inclusion des headers
include_directories(include)

# Exécutable principal
add_executable(infodrone_parser
    src/main.cpp
)
