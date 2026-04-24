cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "nan-asm-compiler")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# add_compile_options(-fconcepts)
add_compile_options(-fpermissive)

include_directories("./" "../vm" "../asm")

add_executable("nanasm" main.cpp)
target_link_options("nanasm" PUBLIC -static-libgcc -static-libstdc++ -O3 -static)
# set_target_properties("nanasm" PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "D:/so2u/GITHUB/cuba/nan/nanbase/test-asm/test.ns")