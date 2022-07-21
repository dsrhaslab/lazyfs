
cmake_minimum_required(VERSION 3.10)

project(lfscheck)

set (CMAKE_CXX_STANDARD 17)

set (CMAKE_BUILD_TYPE Debug)

# argparse

include(FetchContent)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

# spdlog

FetchContent_Declare(spdlog
    URL             https://github.com/gabime/spdlog/archive/v1.10.0.tar.gz
    UPDATE_COMMAND  ""
    INSTALL_COMMAND ""
)

FetchContent_MakeAvailable(spdlog)
set_target_properties(spdlog PROPERTIES POSITION_INDEPENDENT_CODE ON)

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_executable(lfscheck ${SOURCES})

target_link_libraries(lfscheck spdlog argparse Threads::Threads)