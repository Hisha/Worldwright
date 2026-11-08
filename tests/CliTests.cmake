# CTest helper for exercising the built command-line inspector.
#
# Required variables:
#   CLI          - path to worldwright-wdt-info
#   FIXTURE_DIR  - directory produced by worldwright-wdt-test-fixtures

if(NOT DEFINED CLI OR NOT EXISTS "${CLI}")
    message(FATAL_ERROR "CLI executable not found: '${CLI}'")
endif()
if(NOT DEFINED FIXTURE_DIR OR NOT IS_DIRECTORY "${FIXTURE_DIR}")
    message(FATAL_ERROR "Fixture directory not found: '${FIXTURE_DIR}'")
endif()

function(expect_success name)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR
            "${name}: expected exit code 0, got '${result}'\n"
            "stdout:\n${output}\n"
            "stderr:\n${error_output}")
    endif()
    set(LAST_OUTPUT "${output}" PARENT_SCOPE)
    set(LAST_ERROR "${error_output}" PARENT_SCOPE)
endfunction()

function(expect_failure name)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
    )
    if(result STREQUAL "0")
        message(FATAL_ERROR
            "${name}: expected a nonzero exit code\n"
            "stdout:\n${output}\n"
            "stderr:\n${error_output}")
    endif()
endfunction()

function(expect_contains label text needle)
    string(FIND "${text}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "${label}: expected output to contain '${needle}'\n"
            "actual output:\n${text}")
    endif()
endfunction()

expect_success("nine tile info"
    "${CLI}" "${FIXTURE_DIR}/nine_tiles_30_30_to_32_32.wdt")
set(nine_info "${LAST_OUTPUT}")
expect_contains("nine tile info" "${nine_info}" "Worldwright WDT Inspector")
expect_contains("nine tile info" "${nine_info}" "Version: 18")
expect_contains("nine tile info" "${nine_info}" "Grid: 64 x 64")
expect_contains("nine tile info" "${nine_info}" "Existing tiles: 9")
expect_contains("nine tile info" "${nine_info}" "Tile bounds: (30,30) to (32,32)")

expect_success("nine tile list"
    "${CLI}" "${FIXTURE_DIR}/nine_tiles_30_30_to_32_32.wdt" --list)
set(nine_list "${LAST_OUTPUT}")
foreach(y RANGE 30 32)
    foreach(x RANGE 30 32)
        expect_contains("nine tile list" "${nine_list}" "(${x},${y})")
    endforeach()
endforeach()

expect_success("empty tile info"
    "${CLI}" "${FIXTURE_DIR}/empty_grid.wdt")
set(empty_info "${LAST_OUTPUT}")
expect_contains("empty tile info" "${empty_info}" "Existing tiles: 0")
expect_contains("empty tile info" "${empty_info}" "Tile bounds: none")

expect_success("one tile info"
    "${CLI}" "${FIXTURE_DIR}/one_tile_30_30.wdt")
set(one_info "${LAST_OUTPUT}")
expect_contains("one tile info" "${one_info}" "Existing tiles: 1")
expect_contains("one tile info" "${one_info}" "Tile bounds: (30,30) to (30,30)")

expect_success("unknown chunk info"
    "${CLI}" "${FIXTURE_DIR}/unknown_chunks.wdt")
expect_contains("unknown chunk info" "${LAST_OUTPUT}" "Existing tiles: 1")

expect_success("help"
    "${CLI}" --help)
expect_contains("help" "${LAST_OUTPUT}" "Usage:")

expect_failure("no arguments"
    "${CLI}")
expect_failure("unknown option"
    "${CLI}" "${FIXTURE_DIR}/empty_grid.wdt" --bogus)
expect_failure("missing file"
    "${CLI}" "${FIXTURE_DIR}/does-not-exist.wdt")
expect_failure("unsupported version"
    "${CLI}" "${FIXTURE_DIR}/invalid_version_17.wdt")
expect_failure("truncated MAIN"
    "${CLI}" "${FIXTURE_DIR}/truncated_main.wdt")
expect_failure("missing MVER"
    "${CLI}" "${FIXTURE_DIR}/missing_mver.wdt")
expect_failure("missing MPHD"
    "${CLI}" "${FIXTURE_DIR}/missing_mphd.wdt")
expect_failure("missing MAIN"
    "${CLI}" "${FIXTURE_DIR}/missing_main.wdt")
expect_failure("WMO-only WDT"
    "${CLI}" "${FIXTURE_DIR}/wmo_only.wdt")
expect_failure("trailing garbage"
    "${CLI}" "${FIXTURE_DIR}/trailing_garbage.wdt")
expect_failure("invalid MVER size"
    "${CLI}" "${FIXTURE_DIR}/invalid_mver_size.wdt")
expect_failure("invalid MPHD size"
    "${CLI}" "${FIXTURE_DIR}/invalid_mphd_size.wdt")
expect_failure("invalid MAIN size"
    "${CLI}" "${FIXTURE_DIR}/invalid_main_size.wdt")

message(STATUS "CLI checks passed")
