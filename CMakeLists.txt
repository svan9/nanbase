cmake_minimum_required(VERSION 3.16)

# set(RELEASE 0)
add_compile_definitions("DEBUG=$<CONFIG:Debug>")

set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")
# set(CMAKE_CXX_FLAGS "-fsanitize=address")

include_directories("${CMAKE_SOURCE_DIR}/mewlib")
include_directories("${CMAKE_SOURCE_DIR}/vm")

# add_subdirectory(libzip)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(vm)
add_subdirectory(asm)

# add_custom_target(copy_bin ALL
# 	COMMAND ${CMAKE_COMMAND} -E copy_directory
# 	${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/../build-win
# 	COMMENT "Copying bin folder to build-win"
# )

