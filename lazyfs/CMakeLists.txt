
# following this source: http://derekmolloy.ie/hello-world-introductions-to-cmake/

cmake_minimum_required(VERSION 3.10)

project(lazyfs VERSION 0.0.1)

include_directories(include)

link_directories(../libs/libpcache/build)
include_directories(../libs/libpcache/include)
set(CMAKE_BUILD_TYPE Debug)

find_package(Threads REQUIRED)

add_executable(${CMAKE_PROJECT_NAME})

target_sources(
    ${CMAKE_PROJECT_NAME}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include/lazyfs/lazyfs.hpp
    ${PROJECT_SOURCE_DIR}/include/lazyfs/fusepp/Fuse.cpp
    ${PROJECT_SOURCE_DIR}/include/lazyfs/fusepp/Fuse-impl.h
    ${PROJECT_SOURCE_DIR}/include/lazyfs/fusepp/Fuse.h
)

target_sources(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
    src/lazyfs.cpp
    src/main.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} fuse3 Threads::Threads libpcache.so)