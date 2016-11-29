cmake_minimum_required(VERSION 3.6)
project(gros_fs)

set(CMAKE_CXX_STANDARD 11)

INCLUDE(FindPkgConfig)
pkg_check_modules(FUSE REQUIRED fuse)

if (FUSE_FOUND)

set(SOURCE_FILES
        src/bitmap.cpp
        src/disk.cpp
        src/files.cpp
        src/fuse_calls.cpp
        src/grosfs.cpp
        src/main.cpp)

set(INCLUDE_FILES
        include/catch.hpp
        ${FUSE_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS -Wall)
set(CMAKE_CXX_FLAGS -g)

add_definitions(${FUSE_DEFINITIONS})
include_directories(AFTER SYSTEM ${INCLUDE_FILES})
add_executable(grosfs ${SOURCE_FILES})

include_directories(${FUSE_INCLUDE_DIRS})
link_directories(${FUSE_LIBRARY_DIRS})
target_link_libraries(grosfs ${FUSE_LIBRARIES})
set(CMAKE_CXX_FLAGS -D_FILE_OFFSET_BITS=64)


endif()