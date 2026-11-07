cmake_minimum_required(VERSION 3.22)
project(nervous_jni LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Chemin vers ton moteur (dossier core/ à la racine du dépôt)
set(CORE_DIR ${CMAKE_SOURCE_DIR}/../../../../core)

add_library(nervous_core STATIC
        ${CORE_DIR}/Board.cpp
        ${CORE_DIR}/Rules.cpp
        ${CORE_DIR}/Game.cpp
)
target_include_directories(nervous_core PUBLIC ${CORE_DIR})

# Bibliothèque partagée chargée par Kotlin
add_library(nervous_jni SHARED native-lib.cpp)
target_link_libraries(nervous_jni PRIVATE nervous_core log)