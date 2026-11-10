cmake_minimum_required(VERSION 3.14)
project(NervousGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(nervous_core
        core/Board.cpp
        core/Rules.cpp
        core/Game.cpp
        core/MctsAi.cpp
        reseau/Protocol.cpp
        reseau/Server.cpp
        reseau/NetClient.cpp
)
target_include_directories(nervous_core PUBLIC core)
target_compile_options(nervous_core PRIVATE -Wall -Wextra)

# Tests
enable_testing()
add_executable(test_rules tests/test_rules.cpp)
target_link_libraries(test_rules PRIVATE nervous_core)
add_test(NAME rules COMMAND test_rules)

# Server
add_executable(nervous_server reseau/Server.cpp)
target_link_libraries(nervous_server PRIVATE nervous_core)

# Interface graphique SDL3
find_package(SDL3 REQUIRED)
add_executable(nervous_game
        src/main.cpp
        src/GameScreen.cpp
        src/MenuScreen.cpp
)
target_include_directories(nervous_game PRIVATE src)
target_link_libraries(nervous_game PRIVATE nervous_core SDL3::SDL3)