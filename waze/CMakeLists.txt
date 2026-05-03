cmake_minimum_required(VERSION 3.16)
project(waze LANGUAGES CXX)
set(PROJECT_VERSION 1A00F)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")
include_directories("${CMAKE_SOURCE_DIR}/mewlib" "./" "../vm" "../asm" "../code_gen")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-fconcepts)
add_compile_options(-fpermissive)
add_compile_options(-fopenmp)

set(PROJECT_NAME "waze")


add_executable(${PROJECT_NAME} "main.cpp")
target_include_directories(${PROJECT_NAME} PUBLIC "./")


target_link_libraries(${PROJECT_NAME} PUBLIC mewlib -O3 -pthread)

install(TARGETS ${PROJECT_NAME})

