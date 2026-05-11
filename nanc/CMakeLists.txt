cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "nanc")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-fpermissive)

include_directories("./" "../vm" "../asm" "../mewlib" "../code_gen")

add_executable(${PROJECT_NAME} "main.cpp")
target_link_libraries(${PROJECT_NAME} PUBLIC mewlib)
target_link_options(${PROJECT_NAME} PUBLIC -O3)

install(TARGETS ${PROJECT_NAME})