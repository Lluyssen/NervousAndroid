cmake_minimum_required(VERSION 3.14)
project(NervousGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Moteur de règles (bibliothèque sans dépendance)
add_library(nervous_core
        core/Board.cpp
        core/Rules.cpp
        core/Game.cpp
)
target_include_directories(nervous_core PUBLIC core)
target_compile_options(nervous_core PRIVATE -Wall -Wextra)

# Tests
enable_testing()
add_executable(test_rules tests/test_rules.cpp)
target_link_libraries(test_rules PRIVATE nervous_core)
add_test(NAME rules COMMAND test_rules)