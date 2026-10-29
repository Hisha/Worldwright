cmake_minimum_required(VERSION 3.10)

# Project name and version
project(WorldwrightWdtReader VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type detection
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wshadow -Wconversion)
    endif()
endif()

# ============================================================================
# Library target
# ============================================================================
add_library(WorldwrightWdtReader
    src/worldwright/WdtParser.cpp
    src/worldwright/WdtTileMap.cpp
    src/worldwright/Worldwright.cpp
)

# Add include directory for library
target_include_directories(WorldwrightWdtReader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worldwright
)

# ============================================================================
# CLI executable
# ============================================================================
add_executable(worldwright-wdt-info
    src/worldwright/main.cpp
)

target_link_libraries(worldwright-wdt-info PRIVATE WorldwrightWdtReader)

# ============================================================================
# Installation
# ============================================================================
install(TARGETS WorldwrightWdtReader
    EXPORT WorldwrightWdtReaderTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS worldwright-wdt-info
    RUNTIME DESTINATION bin
)

# ============================================================================
# Tests
# ============================================================================
enable_testing()

# Create test directory structure
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/CTestTests")

# Add test executable
add_executable(WDT_tests tests/WDT_tests.cpp)
target_link_libraries(WDT_tests PRIVATE WorldwrightWdtReader)

# Enable CTest
include(CTest)

# ============================================================================
# Compiler-specific test settings
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Enable assertions for tests
    add_compile_options(-DDEBUG)
endif()

message(STATUS "Worldwright WDT Reader build complete")
message(STATUS "Library: WorldwrightWdtReader")
message(STATUS "CLI: worldwright-wdt-info")
message(STATUS "Tests: WDT_tests")
