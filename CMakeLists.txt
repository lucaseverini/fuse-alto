cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(fuse-alto VERSION 0.3.1 LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -D_FILE_OFFSET_BITS=64")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Werror --pedantic -g -DDEBUG=1")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

configure_file(
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
    )
# Add the binary tree to the search path for include files
include_directories("${PROJECT_BINARY_DIR}")

find_package(FUSE REQUIRED)

include_directories("${FUSE_INCLUDE_DIR}")
add_executable(fuse-alto fuse-alto.cpp altofs.cpp fileinfo.cpp)
target_link_libraries(fuse-alto ${FUSE_LIBRARIES})

install(TARGETS fuse-alto DESTINATION bin)
install(FILES "${PROJECT_SOURCE_DIR}/README.md" DESTINATION share/doc/fuse-alto)
