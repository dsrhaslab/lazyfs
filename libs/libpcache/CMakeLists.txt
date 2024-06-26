cmake_minimum_required(VERSION 3.10)

project(pcache VERSION 0.0.1)

set (CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    toml
    GIT_REPOSITORY https://github.com/ToruNiina/toml11
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(toml)

FetchContent_Declare(spdlog
    URL             https://github.com/gabime/spdlog/archive/v1.10.0.tar.gz
    UPDATE_COMMAND  ""
    INSTALL_COMMAND ""
)

FetchContent_MakeAvailable(spdlog)

set_target_properties(spdlog PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(warn_opts -Wall -Wextra)

add_library (
    pcache SHARED ""
)

target_compile_options(
    pcache 
    PRIVATE ${warn_opts}
)

set_target_properties(
    pcache 
    PROPERTIES
    VERSION ${PROJECT_VERSION_MAJOR}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_include_directories(
    pcache PUBLIC include
)

target_sources(
    pcache
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include/cache/engine/backends/custom/custom_cache.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/engine/page_cache.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/engine/page/page.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/engine/page/block_offsets.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/constants/constants.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/cache.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/item/item.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/item/metadata.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/item/data.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/item/block_info.hpp
    ${PROJECT_SOURCE_DIR}/include/cache/config/config.hpp
    ${PROJECT_SOURCE_DIR}/include/faults/faults.hpp
)

target_sources(
    pcache
    PRIVATE
    src/engine/backends/custom/custom_cache.cpp
    src/engine/page/block_offsets.cpp
    src/engine/page/page.cpp
    src/cache.cpp
    src/item/metadata.cpp
    src/item/block_info.cpp
    src/item/data.cpp
    src/item/item.cpp
    src/config/config.cpp
    src/faults.cpp
)

target_link_libraries(pcache toml11::toml11 spdlog)

include(GNUInstallDirs)

install(
    TARGETS pcache
    EXPORT pcacheTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/cache TYPE INCLUDE)
