cmake_minimum_required(VERSION 3.16)

project(Worldwright VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WORLDWRIGHT_BUILD_GUI "Build the Qt 6 graphical world editor" ON)

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

add_library(WorldwrightEditorCore
    src/worldwright/EditorInteraction.cpp
    src/worldwright/TerrainDetailInteraction.cpp
    src/worldwright/TerrainHeightEditor.cpp
    src/worldwright/WorldEditorModel.cpp
    src/worldwright/WorldExporter.cpp
)
add_library(Worldwright::EditorCore ALIAS WorldwrightEditorCore)
target_compile_features(WorldwrightEditorCore PUBLIC cxx_std_17)
target_include_directories(WorldwrightEditorCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(WorldwrightEditorCore PUBLIC
    WorldwrightWdtWriter
    WorldwrightAdtWriter
)
worldwright_enable_warnings(WorldwrightEditorCore)

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
# Optional Qt graphical editor
# ---------------------------------------------------------------------------
if(WORLDWRIGHT_BUILD_GUI)
    find_package(Qt6 6.2 COMPONENTS Widgets QUIET)
    if(Qt6Widgets_FOUND)
        add_library(WorldwrightGui STATIC
            src/worldwright/gui/MainWindow.cpp
            src/worldwright/gui/MainWindow.h
            src/worldwright/gui/TerrainDetailWidget.cpp
            src/worldwright/gui/TerrainDetailWidget.h
            src/worldwright/gui/WorldMapWidget.cpp
            src/worldwright/gui/WorldMapWidget.h
        )
        target_link_libraries(WorldwrightGui PUBLIC
            WorldwrightEditorCore
            Qt6::Widgets
        )
        target_include_directories(WorldwrightGui PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/worldwright/gui
        )
        worldwright_enable_warnings(WorldwrightGui)
        add_executable(worldwright src/worldwright/gui/gui_main.cpp)
        target_link_libraries(worldwright PRIVATE WorldwrightGui)
        worldwright_enable_warnings(worldwright)
    else()
        message(WARNING
            "Qt 6 Widgets development files were not found; the worldwright "
            "GUI target will not be built. On Ubuntu install qt6-base-dev "
            "(and qt6-base-dev-tools), then reconfigure. CLI tools and tests "
            "remain available. Set WORLDWRIGHT_BUILD_GUI=OFF for an explicit "
            "headless build.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
install(TARGETS WorldwrightWdtReader WorldwrightWdtWriter WorldwrightAdtWriter
                WorldwrightEditorCore
                worldwright-wdt-info worldwright-wdt-create
                worldwright-adt-create
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(TARGET worldwright)
    install(TARGETS worldwright RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
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

    add_executable(worldwright-adt-integrity-tests
        tests/AdtTerrainIntegrityTests.cpp
    )
    target_link_libraries(worldwright-adt-integrity-tests PRIVATE
        WorldwrightEditorCore
    )
    worldwright_enable_warnings(worldwright-adt-integrity-tests)
    add_test(NAME adt_terrain_integrity
             COMMAND worldwright-adt-integrity-tests)

    add_executable(worldwright-w005c-fixture-generator
        tests/W005CFixtureGenerator.cpp
    )
    target_link_libraries(worldwright-w005c-fixture-generator PRIVATE
        WorldwrightEditorCore
    )
    worldwright_enable_warnings(worldwright-w005c-fixture-generator)

    add_test(NAME adt_create_cli
        COMMAND ${CMAKE_COMMAND}
                "-DCREATE_CLI=$<TARGET_FILE:worldwright-adt-create>"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/adt-create-cli-work"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/AdtCreateCliTests.cmake"
    )

    add_executable(worldwright-editor-core-tests
        tests/WorldEditorTests.cpp
    )
    target_link_libraries(worldwright-editor-core-tests PRIVATE
        WorldwrightEditorCore
        WorldwrightWdtReader
    )
    worldwright_enable_warnings(worldwright-editor-core-tests)
    add_test(NAME editor_core_unit COMMAND worldwright-editor-core-tests)

    add_executable(worldwright-terrain-editor-tests
        tests/TerrainHeightEditorTests.cpp
    )
    target_link_libraries(worldwright-terrain-editor-tests PRIVATE
        WorldwrightEditorCore
    )
    worldwright_enable_warnings(worldwright-terrain-editor-tests)
    add_test(NAME terrain_editor_unit COMMAND worldwright-terrain-editor-tests)

    add_executable(worldwright-terrain-detail-tests
        tests/TerrainDetailInteractionTests.cpp
    )
    target_link_libraries(worldwright-terrain-detail-tests PRIVATE
        WorldwrightEditorCore
    )
    worldwright_enable_warnings(worldwright-terrain-detail-tests)
    add_test(NAME terrain_detail_unit COMMAND worldwright-terrain-detail-tests)

    if(TARGET WorldwrightGui)
        add_executable(worldwright-terrain-detail-gui-tests
            tests/TerrainDetailGuiTests.cpp
        )
        target_link_libraries(worldwright-terrain-detail-gui-tests PRIVATE
            WorldwrightGui
        )
        worldwright_enable_warnings(worldwright-terrain-detail-gui-tests)
        add_test(NAME terrain_detail_gui_offscreen
                 COMMAND worldwright-terrain-detail-gui-tests)
        set_tests_properties(terrain_detail_gui_offscreen PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()
