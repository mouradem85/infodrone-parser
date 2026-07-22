cmake_minimum_required(VERSION 3.25)
project(InfoDroneParserProject CXX)

# Exigence de C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Intégration de GoogleTest (mis en premier pour l'avoir partout) ---
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Binaire principal
add_executable(drone_parser
    src/main.cpp
    src/InfoDroneParser.cpp
    src/PcapDataSource.cpp
)

target_include_directories(drone_parser PRIVATE include)

# On autorise drone_parser à voir les headers de GoogleTest (pour FRIEND_TEST)
target_link_libraries(drone_parser PRIVATE gtest)

# --- Configuration des tests unitaires ---
enable_testing()

add_executable(unit_tests
    tests/test_parser.cpp
    src/InfoDroneParser.cpp
)

target_include_directories(unit_tests PRIVATE include)

target_link_libraries(unit_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)