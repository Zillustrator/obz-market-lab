cmake_minimum_required(VERSION 3.21)

project(obz_market_lab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OBZ_MARKET_LAB_BUILD_TESTS "Build Obz Market Lab tests" ON)
option(OBZ_MARKET_LAB_BUILD_BENCHMARKS "Build Obz Market Lab benchmarks" OFF)
option(OBZ_MARKET_LAB_BUILD_APPS "Build Obz Market Lab demo applications" ON)
option(OBZ_MARKET_LAB_ENABLE_TRACY "Enable Tracy instrumentation" OFF)
set(OBZ_MARKET_LAB_OBZ_SOURCE_DIR
    ""
    CACHE PATH
    "Optional local ObzLib source directory. When empty, ObzLib is fetched from Git."
)

include(FetchContent)

set(OBZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OBZ_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

if (OBZ_MARKET_LAB_OBZ_SOURCE_DIR)
    FetchContent_Declare(
        obz
        SOURCE_DIR "${OBZ_MARKET_LAB_OBZ_SOURCE_DIR}"
    )
else()
    FetchContent_Declare(
        obz
        GIT_REPOSITORY https://github.com/Zillustrator/obz.git
        GIT_TAG v0.1.0
    )
endif()

FetchContent_MakeAvailable(obz)

if (OBZ_MARKET_LAB_ENABLE_TRACY)
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.13.1
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(tracy)
endif()

add_subdirectory(cpp/engine)
add_subdirectory(cpp/pipeline)

if (OBZ_MARKET_LAB_BUILD_APPS)
    add_subdirectory(apps)
endif()

if (OBZ_MARKET_LAB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (OBZ_MARKET_LAB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
