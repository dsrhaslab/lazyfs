
# following this source: http://derekmolloy.ie/hello-world-introductions-to-cmake/

cmake_minimum_required(VERSION 3.10)

project(lazyfs VERSION 0.0.1)

set (CMAKE_CXX_STANDARD 17)

include_directories(include)

link_directories(../libs/libpcache/build)
include_directories(../libs/libpcache/include)
set(CMAKE_BUILD_TYPE Release)

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

# ------------------ LazyFS tests ------------------

option(LAZYFS_BUILD_TESTS "Build LazyFS unit tests" ON)

if (LAZYFS_BUILD_TESTS)

    enable_testing()

    # Import Google GTest

    include(FetchContent)

    FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.11.0
    )
    FetchContent_MakeAvailable(googletest)

    add_library(GTest::GTest INTERFACE IMPORTED)
    target_link_libraries(GTest::GTest INTERFACE gtest_main)

    add_executable(test_write tests/test_write.cpp)
    add_executable(test_consistency_clear_cache tests/test_consistency_clear_cache.cpp)
    add_executable(test_truncate_clear_cache tests/test_truncate_clear_cache.cpp)
    add_executable(test_file_links_clear_cache tests/test_file_links_clear_cache.cpp)
    target_link_libraries(test_write GTest::GTest gtest_main)
    target_link_libraries(test_consistency_clear_cache GTest::GTest gtest_main)
    target_link_libraries(test_truncate_clear_cache GTest::GTest gtest_main)
    target_link_libraries(test_file_links_clear_cache GTest::GTest gtest_main)

endif()