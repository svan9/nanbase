cmake_minimum_required(VERSION 3.16)

add_compile_definitions("DEBUG=$<CONFIG:Debug>")

set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

include_directories("${CMAKE_SOURCE_DIR}/mewlib")
include_directories("${CMAKE_SOURCE_DIR}/vm")

add_subdirectory(mewlib)



set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_subdirectory(vm)
add_subdirectory(asm)
add_subdirectory(code_gen)
add_subdirectory(waze)

# Настройки CPack для создания инсталлятора
set(CPACK_PACKAGE_NAME "NanBase")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NanBase VM and Tools")
set(CPACK_PACKAGE_VENDOR "svan9")
set(CPACK_GENERATOR "NSIS")
set(CPACK_NSIS_MODIFY_PATH ON)  # Добавляет в PATH
include(CPack)

