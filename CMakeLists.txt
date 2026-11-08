cmake_minimum_required(VERSION 3.16)

project(Worldwright VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

function(worldwright_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(WorldwrightWdtReader
    src/worldwright/WdtReader.cpp
)
add_library(Worldwright::WdtReader ALIAS WorldwrightWdtReader)

target_compile_features(WorldwrightWdtReader PUBLIC cxx_std_17)
target_include_directories(WorldwrightWdtReader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
worldwright_enable_warnings(WorldwrightWdtReader)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(WorldwrightWdtReader PUBLIC stdc++fs)
endif()

add_library(WorldwrightWdtWriter
    src/worldwright/WorldDefinition.cpp
    src/worldwright/WdtWriter.cpp
)
add_library(Worldwright::WdtWriter ALIAS WorldwrightWdtWriter)

target_compile_features(WorldwrightWdtWriter PUBLIC cxx_std_17)
target_include_directories(WorldwrightWdtWriter PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
worldwright_enable_warnings(WorldwrightWdtWriter)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(WorldwrightWdtWriter PUBLIC stdc++fs)
endif()

add_library(WorldwrightAdtWriter
    src/worldwright/TerrainDefinition.cpp
    src/worldwright/AdtWriter.cpp
)
add_library(Worldwright::AdtWriter ALIAS WorldwrightAdtWriter)
target_compile_features(WorldwrightAdtWriter PUBLIC cxx_std_17)
target_include_directories(WorldwrightAdtWriter PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
worldwright_enable_warnings(WorldwrightAdtWriter)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(WorldwrightAdtWriter PUBLIC stdc++fs)
endif()

# ---------------------------------------------------------------------------
# Command-line tools
# ---------------------------------------------------------------------------
add_executable(worldwright-wdt-info
    src/worldwright/main.cpp
)
target_link_libraries(worldwright-wdt-info PRIVATE WorldwrightWdtReader)
worldwright_enable_warnings(worldwright-wdt-info)

add_executable(worldwright-wdt-create
    src/worldwright/create_main.cpp
)
target_link_libraries(worldwright-wdt-create PRIVATE WorldwrightWdtWriter)
worldwright_enable_warnings(worldwright-wdt-create)

add_executable(worldwright-adt-create
    src/worldwright/adt_create_main.cpp
)
target_link_libraries(worldwright-adt-create PRIVATE
    WorldwrightAdtWriter
    WorldwrightWdtWriter
)
worldwright_enable_warnings(worldwright-adt-create)

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
install(TARGETS WorldwrightWdtReader WorldwrightWdtWriter WorldwrightAdtWriter
                worldwright-wdt-info worldwright-wdt-create
                worldwright-adt-create
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    add_executable(worldwright-wdt-tests
        tests/WdtReaderTests.cpp
    )
    target_link_libraries(worldwright-wdt-tests PRIVATE WorldwrightWdtReader)
    target_include_directories(worldwright-wdt-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    worldwright_enable_warnings(worldwright-wdt-tests)
    add_test(NAME wdt_reader_unit COMMAND worldwright-wdt-tests)

    add_executable(worldwright-wdt-test-fixtures
        tests/WdtFixtureGenerator.cpp
    )
    target_include_directories(worldwright-wdt-test-fixtures PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    worldwright_enable_warnings(worldwright-wdt-test-fixtures)

    add_executable(worldwright-wdt-writer-tests
        tests/WdtWriterTests.cpp
    )
    target_link_libraries(worldwright-wdt-writer-tests PRIVATE
        WorldwrightWdtWriter
        WorldwrightWdtReader
    )
    target_include_directories(worldwright-wdt-writer-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    worldwright_enable_warnings(worldwright-wdt-writer-tests)
    add_test(NAME wdt_writer_unit COMMAND worldwright-wdt-writer-tests)

    add_test(NAME wdt_fixtures
        COMMAND worldwright-wdt-test-fixtures
                "${CMAKE_CURRENT_BINARY_DIR}/test-fixtures"
    )
    set_tests_properties(wdt_fixtures PROPERTIES
        FIXTURES_SETUP wdt_test_fixtures
    )

    add_test(NAME wdt_cli
        COMMAND ${CMAKE_COMMAND}
                "-DCLI=$<TARGET_FILE:worldwright-wdt-info>"
                "-DFIXTURE_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-fixtures"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/CliTests.cmake"
    )
    set_tests_properties(wdt_cli PROPERTIES
        FIXTURES_REQUIRED wdt_test_fixtures
    )

    add_test(NAME wdt_create_cli
        COMMAND ${CMAKE_COMMAND}
                "-DCREATE_CLI=$<TARGET_FILE:worldwright-wdt-create>"
                "-DINFO_CLI=$<TARGET_FILE:worldwright-wdt-info>"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/create-cli-work"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/WdtCreateCliTests.cmake"
    )

    add_executable(worldwright-adt-writer-tests
        tests/AdtWriterTests.cpp
    )
    target_link_libraries(worldwright-adt-writer-tests PRIVATE
        WorldwrightAdtWriter
    )
    worldwright_enable_warnings(worldwright-adt-writer-tests)
    add_test(NAME adt_writer_unit COMMAND worldwright-adt-writer-tests)

    add_test(NAME adt_create_cli
        COMMAND ${CMAKE_COMMAND}
                "-DCREATE_CLI=$<TARGET_FILE:worldwright-adt-create>"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/adt-create-cli-work"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/AdtCreateCliTests.cmake"
    )
endif()
