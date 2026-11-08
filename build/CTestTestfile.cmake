# CMake generated Testfile for 
# Source directory: /home/smithkt/git/Worldwright
# Build directory: /home/smithkt/git/Worldwright/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(wdt_reader_unit "/home/smithkt/git/Worldwright/build/worldwright-wdt-tests")
set_tests_properties(wdt_reader_unit PROPERTIES  _BACKTRACE_TRIPLES "/home/smithkt/git/Worldwright/CMakeLists.txt;126;add_test;/home/smithkt/git/Worldwright/CMakeLists.txt;0;")
add_test(wdt_writer_unit "/home/smithkt/git/Worldwright/build/worldwright-wdt-writer-tests")
set_tests_properties(wdt_writer_unit PROPERTIES  _BACKTRACE_TRIPLES "/home/smithkt/git/Worldwright/CMakeLists.txt;147;add_test;/home/smithkt/git/Worldwright/CMakeLists.txt;0;")
add_test(wdt_fixtures "/home/smithkt/git/Worldwright/build/worldwright-wdt-test-fixtures" "/home/smithkt/git/Worldwright/build/test-fixtures")
set_tests_properties(wdt_fixtures PROPERTIES  FIXTURES_SETUP "wdt_test_fixtures" _BACKTRACE_TRIPLES "/home/smithkt/git/Worldwright/CMakeLists.txt;149;add_test;/home/smithkt/git/Worldwright/CMakeLists.txt;0;")
add_test(wdt_cli "/usr/bin/cmake" "-DCLI=/home/smithkt/git/Worldwright/build/worldwright-wdt-info" "-DFIXTURE_DIR=/home/smithkt/git/Worldwright/build/test-fixtures" "-P" "/home/smithkt/git/Worldwright/tests/CliTests.cmake")
set_tests_properties(wdt_cli PROPERTIES  FIXTURES_REQUIRED "wdt_test_fixtures" _BACKTRACE_TRIPLES "/home/smithkt/git/Worldwright/CMakeLists.txt;157;add_test;/home/smithkt/git/Worldwright/CMakeLists.txt;0;")
add_test(wdt_create_cli "/usr/bin/cmake" "-DCREATE_CLI=/home/smithkt/git/Worldwright/build/worldwright-wdt-create" "-DINFO_CLI=/home/smithkt/git/Worldwright/build/worldwright-wdt-info" "-DWORK_DIR=/home/smithkt/git/Worldwright/build/create-cli-work" "-P" "/home/smithkt/git/Worldwright/tests/WdtCreateCliTests.cmake")
set_tests_properties(wdt_create_cli PROPERTIES  _BACKTRACE_TRIPLES "/home/smithkt/git/Worldwright/CMakeLists.txt;167;add_test;/home/smithkt/git/Worldwright/CMakeLists.txt;0;")
add_test(adt_writer_unit "/home/smithkt/git/Worldwright/build/worldwright-adt-writer-tests")
set_tests_properties(adt_writer_unit PROPERTIES  _BACKTRACE_TRIPLES "/home/smithkt/git/Worldwright/CMakeLists.txt;182;add_test;/home/smithkt/git/Worldwright/CMakeLists.txt;0;")
add_test(adt_create_cli "/usr/bin/cmake" "-DCREATE_CLI=/home/smithkt/git/Worldwright/build/worldwright-adt-create" "-DWORK_DIR=/home/smithkt/git/Worldwright/build/adt-create-cli-work" "-P" "/home/smithkt/git/Worldwright/tests/AdtCreateCliTests.cmake")
set_tests_properties(adt_create_cli PROPERTIES  _BACKTRACE_TRIPLES "/home/smithkt/git/Worldwright/CMakeLists.txt;184;add_test;/home/smithkt/git/Worldwright/CMakeLists.txt;0;")
