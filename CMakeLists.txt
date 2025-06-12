cmake_minimum_required(VERSION 3.10)

project(libpcache_lazyfs VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)

# Add the subdirectories
add_subdirectory(libs/libpcache)
add_subdirectory(lazyfs)