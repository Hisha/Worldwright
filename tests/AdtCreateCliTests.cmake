if(NOT DEFINED CREATE_CLI OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "CREATE_CLI and WORK_DIR are required")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/single" "${WORK_DIR}/empty" "${WORK_DIR}/nine" "${WORK_DIR}/sparse")

execute_process(COMMAND "${CREATE_CLI}" --output "${WORK_DIR}/single/One_0_0.adt" --tile 0 0 --height -25
                RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT EXISTS "${WORK_DIR}/single/One_0_0.adt")
    message(FATAL_ERROR "single generation failed: ${output}${error}")
endif()
file(SIZE "${WORK_DIR}/single/One_0_0.adt" size)
if(NOT size EQUAL 320708)
    message(FATAL_ERROR "unexpected single ADT size: ${size}")
endif()

execute_process(COMMAND "${CREATE_CLI}" --world Empty --output-dir "${WORK_DIR}/empty" --height 0
                RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR NOT EXISTS "${WORK_DIR}/empty/Empty.wdt")
    message(FATAL_ERROR "empty world failed: ${error}")
endif()
file(GLOB empty_adts "${WORK_DIR}/empty/*.adt")
if(empty_adts)
    message(FATAL_ERROR "empty world unexpectedly generated ADTs")
endif()

execute_process(COMMAND "${CREATE_CLI}" --world NewWorld --output-dir "${WORK_DIR}/nine" --rectangle 30 30 32 32 --height 100
                RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "nine-tile generation failed: ${error}")
endif()
file(GLOB nine_adts "${WORK_DIR}/nine/*.adt")
list(LENGTH nine_adts count)
if(NOT count EQUAL 9 OR NOT EXISTS "${WORK_DIR}/nine/NewWorld.wdt" OR
   NOT EXISTS "${WORK_DIR}/nine/NewWorld_30_30.adt" OR
   NOT EXISTS "${WORK_DIR}/nine/NewWorld_32_32.adt")
    message(FATAL_ERROR "nine-tile filenames/count are wrong: ${count}")
endif()

execute_process(COMMAND "${CREATE_CLI}" --world Sparse --output-dir "${WORK_DIR}/sparse"
                        --tile 0 0 --tile 63 63 --tile 12 34 --height 1.5
                RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "sparse generation failed: ${error}")
endif()
file(GLOB sparse_adts "${WORK_DIR}/sparse/*.adt")
list(LENGTH sparse_adts count)
if(NOT count EQUAL 3 OR NOT EXISTS "${WORK_DIR}/sparse/Sparse_0_0.adt" OR
   NOT EXISTS "${WORK_DIR}/sparse/Sparse_63_63.adt")
    message(FATAL_ERROR "sparse/boundary filenames are wrong")
endif()

execute_process(COMMAND "${CREATE_CLI}" --output "${WORK_DIR}/single/bad.adt" --tile 1 1 --height nan
                RESULT_VARIABLE result)
if(result EQUAL 0)
    message(FATAL_ERROR "NaN elevation was accepted")
endif()
execute_process(COMMAND "${CREATE_CLI}" --output "${WORK_DIR}/single/One_0_0.adt" --tile 0 0 --height 0
                RESULT_VARIABLE result)
if(result EQUAL 0)
    message(FATAL_ERROR "existing file was overwritten without --overwrite")
endif()
execute_process(COMMAND "${CREATE_CLI}" --output "${WORK_DIR}/single/One_0_0.adt" --tile 0 0 --height 0 --overwrite
                RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "explicit overwrite failed: ${error}")
endif()
