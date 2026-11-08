# CTest helper for exercising worldwright-wdt-create.
#
# Required variables:
#   CREATE_CLI - path to worldwright-wdt-create
#   INFO_CLI   - path to worldwright-wdt-info
#   WORK_DIR   - scratch directory for generated files

if(NOT DEFINED CREATE_CLI OR NOT EXISTS "${CREATE_CLI}")
    message(FATAL_ERROR "Create CLI executable not found: '${CREATE_CLI}'")
endif()
if(NOT DEFINED INFO_CLI OR NOT EXISTS "${INFO_CLI}")
    message(FATAL_ERROR "Info CLI executable not found: '${INFO_CLI}'")
endif()
if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "WORK_DIR is not defined")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

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
    set(LAST_OUTPUT "${output}" PARENT_SCOPE)
    set(LAST_ERROR "${error_output}" PARENT_SCOPE)
endfunction()

function(expect_contains label text needle)
    string(FIND "${text}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "${label}: expected output to contain '${needle}'\n"
            "actual output:\n${text}")
    endif()
endfunction()

function(expect_file path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "expected file was not created: ${path}")
    endif()
endfunction()

function(expect_no_file path)
    if(EXISTS "${path}")
        message(FATAL_ERROR "unexpected file exists: ${path}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Usage and argument validation
# ---------------------------------------------------------------------------
expect_success("help" "${CREATE_CLI}" --help)
expect_contains("help" "${LAST_OUTPUT}" "Usage:")

expect_failure("no arguments" "${CREATE_CLI}")
expect_failure("unknown option" "${CREATE_CLI}" --bogus)
expect_failure("missing output value" "${CREATE_CLI}" --output)
expect_failure("duplicate output" "${CREATE_CLI}"
    --output "${WORK_DIR}/a.wdt" --output "${WORK_DIR}/b.wdt")
expect_failure("tile missing y" "${CREATE_CLI}"
    --output "${WORK_DIR}/bad.wdt" --tile 1)
expect_failure("tile not an integer" "${CREATE_CLI}"
    --output "${WORK_DIR}/bad.wdt" --tile 3x 4)
expect_failure("tile negative" "${CREATE_CLI}"
    --output "${WORK_DIR}/bad.wdt" --tile -1 0)
expect_failure("tile out of range" "${CREATE_CLI}"
    --output "${WORK_DIR}/bad.wdt" --tile 64 0)
expect_failure("rectangle out of range" "${CREATE_CLI}"
    --output "${WORK_DIR}/bad.wdt" --rectangle 0 0 64 4)
expect_failure("rectangle reversed" "${CREATE_CLI}"
    --output "${WORK_DIR}/bad.wdt" --rectangle 10 10 9 20)
expect_failure("missing output directory" "${CREATE_CLI}"
    --output "${WORK_DIR}/missing/out.wdt")
expect_no_file("${WORK_DIR}/bad.wdt")

# ---------------------------------------------------------------------------
# World creation and inspector round-trip
# ---------------------------------------------------------------------------
set(empty_wdt "${WORK_DIR}/empty.wdt")
expect_success("empty world" "${CREATE_CLI}" --output "${empty_wdt}")
expect_contains("empty world" "${LAST_OUTPUT}" "Enabled tiles: 0")
expect_contains("empty world" "${LAST_OUTPUT}" "Tile bounds: none")
expect_file("${empty_wdt}")

expect_success("empty world info" "${INFO_CLI}" "${empty_wdt}")
expect_contains("empty world info" "${LAST_OUTPUT}" "Version: 18")
expect_contains("empty world info" "${LAST_OUTPUT}" "Grid: 64 x 64")
expect_contains("empty world info" "${LAST_OUTPUT}" "Existing tiles: 0")
expect_contains("empty world info" "${LAST_OUTPUT}" "Tile bounds: none")

set(nine_wdt "${WORK_DIR}/nine.wdt")
expect_success("nine tile rectangle" "${CREATE_CLI}"
    --output "${nine_wdt}" --rectangle 30 30 32 32)
expect_contains("nine tile rectangle" "${LAST_OUTPUT}" "Enabled tiles: 9")
expect_contains("nine tile rectangle" "${LAST_OUTPUT}"
    "Tile bounds: (30,30) to (32,32)")

expect_success("nine tile info" "${INFO_CLI}" "${nine_wdt}")
expect_contains("nine tile info" "${LAST_OUTPUT}" "Existing tiles: 9")
expect_contains("nine tile info" "${LAST_OUTPUT}"
    "Tile bounds: (30,30) to (32,32)")

set(single_wdt "${WORK_DIR}/single.wdt")
expect_success("single tile" "${CREATE_CLI}"
    --output "${single_wdt}" --tile 32 32)
expect_contains("single tile" "${LAST_OUTPUT}" "Enabled tiles: 1")
expect_contains("single tile" "${LAST_OUTPUT}"
    "Tile bounds: (32,32) to (32,32)")

expect_success("single tile info" "${INFO_CLI}" "${single_wdt}")
expect_contains("single tile info" "${LAST_OUTPUT}" "Existing tiles: 1")
expect_contains("single tile info" "${LAST_OUTPUT}"
    "Tile bounds: (32,32) to (32,32)")

set(multi_wdt "${WORK_DIR}/multi.wdt")
expect_success("multiple tiles" "${CREATE_CLI}"
    --output "${multi_wdt}"
    --tile 30 30 --tile 31 30 --tile 32 30)
expect_contains("multiple tiles" "${LAST_OUTPUT}" "Enabled tiles: 3")
expect_contains("multiple tiles" "${LAST_OUTPUT}"
    "Tile bounds: (30,30) to (32,30)")

expect_success("multiple tiles info" "${INFO_CLI}" "${multi_wdt}" --list)
expect_contains("multiple tiles info" "${LAST_OUTPUT}" "Existing tiles: 3")
expect_contains("multiple tiles info" "${LAST_OUTPUT}" "(30,30)")
expect_contains("multiple tiles info" "${LAST_OUTPUT}" "(31,30)")
expect_contains("multiple tiles info" "${LAST_OUTPUT}" "(32,30)")

set(spaces_wdt "${WORK_DIR}/world with spaces.wdt")
expect_success("path with spaces" "${CREATE_CLI}"
    --output "${spaces_wdt}" --tile 0 0 --tile 63 63)
expect_success("path with spaces info" "${INFO_CLI}" "${spaces_wdt}")
expect_contains("path with spaces info" "${LAST_OUTPUT}" "Existing tiles: 2")
expect_contains("path with spaces info" "${LAST_OUTPUT}"
    "Tile bounds: (0,0) to (63,63)")

# ---------------------------------------------------------------------------
# Determinism and overwrite protection
# ---------------------------------------------------------------------------
set(det_a "${WORK_DIR}/det-a.wdt")
set(det_b "${WORK_DIR}/det-b.wdt")
expect_success("determinism a" "${CREATE_CLI}"
    --output "${det_a}" --rectangle 10 10 12 12 --tile 20 20)
expect_success("determinism b" "${CREATE_CLI}"
    --output "${det_b}" --rectangle 10 10 12 12 --tile 20 20)
file(SHA256 "${det_a}" det_a_hash)
file(SHA256 "${det_b}" det_b_hash)
if(NOT det_a_hash STREQUAL det_b_hash)
    message(FATAL_ERROR
        "determinism: identical inputs produced different files\n"
        "${det_a}: ${det_a_hash}\n${det_b}: ${det_b_hash}")
endif()

set(protected_wdt "${WORK_DIR}/protected.wdt")
expect_success("protected setup" "${CREATE_CLI}" --output "${protected_wdt}")
file(SHA256 "${protected_wdt}" protected_before)
expect_failure("overwrite refused" "${CREATE_CLI}"
    --output "${protected_wdt}" --tile 1 1)
expect_contains("overwrite refused" "${LAST_ERROR}" "--overwrite")
file(SHA256 "${protected_wdt}" protected_after)
if(NOT protected_before STREQUAL protected_after)
    message(FATAL_ERROR "overwrite refusal changed the existing file")
endif()

expect_success("overwrite allowed" "${CREATE_CLI}"
    --output "${protected_wdt}" --tile 1 1 --overwrite)
expect_success("overwrite info" "${INFO_CLI}" "${protected_wdt}")
expect_contains("overwrite info" "${LAST_OUTPUT}" "Existing tiles: 1")
expect_contains("overwrite info" "${LAST_OUTPUT}"
    "Tile bounds: (1,1) to (1,1)")

# Output path that is an existing directory must fail cleanly.
file(MAKE_DIRECTORY "${WORK_DIR}/directory.wdt")
expect_failure("output is directory" "${CREATE_CLI}"
    --output "${WORK_DIR}/directory.wdt")
if(NOT IS_DIRECTORY "${WORK_DIR}/directory.wdt")
    message(FATAL_ERROR "output directory was removed or replaced")
endif()

message(STATUS "WDT create CLI checks passed")
