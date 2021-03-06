include(ProcessorCount)
ProcessorCount(N)

set(CTEST_MODEL "Experimental")

set(OPTION_CONFIGURE "-DBUILD_DOCS=OFF -DBUILD_DEPENDENCY_TESTING=on")
set(EXCLUDE_RE "resolv|multistress|glslang-gtests|test-testsuites_all|test-testsuites_default|test-ubjson_all|test-regression_all|test-regression_default|test-msgpack_all|test-msgpack_default|test-json_patch_all|test-json_patch_default|test-json_patch_all|test-inspection_all|test-inspection_default|test-cbor_all|test-cbor_default|test-bson_all|test-unicode_all")

set(CTEST_CONFIGURATION_TYPE "Debug")
set(CTEST_DASHBOARD_ROOT "/local/tmp/$ENV{USER}/hevx-ci")

# don't flag Python DeprecationWarning messages as errors
set (CTEST_CUSTOM_ERROR_EXCEPTION "DeprecationWarning")

if(NOT EXISTS ${CTEST_DASHBOARD_ROOT})
  file(MAKE_DIRECTORY ${CTEST_DASHBOARD_ROOT})
endif()

set(CTEST_SOURCE_DIRECTORY "${CTEST_DASHBOARD_ROOT}/hevx")
set(CTEST_BINARY_DIRECTORY "${CTEST_DASHBOARD_ROOT}/build-${CTEST_CONFIGURATION_TYPE}")

if(NOT EXISTS ${CTEST_SOURCE_DIRECTORY})
  set(CTEST_CHECKOUT_COMMAND "git clone https://github.com/usnistgov/hevx.git")
endif()

set(CTEST_UPDATE_COMMAND "git")

set(_opts "${OPTION_CONFIGURE} -DCMAKE_BUILD_TYPE=${CTEST_CONFIGURATION_TYPE}")
set(CTEST_CONFIGURE_COMMAND "${CMAKE_COMMAND} ${_opts} ${CTEST_SOURCE_DIRECTORY}")

set(CTEST_BUILD_COMMAND "make -j${N}")

message("-- Start dashboard ${CTEST_MODEL} - ${CTEST_CONFIGURATION_TYPE} --")
ctest_start(${CTEST_MODEL} TRACK ${CTEST_MODEL})

message("-- Update ${CTEST_MODEL} - ${CTEST_CONFIGURATION_TYPE} --")
ctest_update(SOURCE "${CTEST_SOURCE_DIRECTORY}")

message("-- Configure ${CTEST_MODEL} - ${CTEST_CONFIGURATION_TYPE} --")
ctest_configure(BUILD "${CTEST_BINARY_DIRECTORY}")

message("-- Build ${CTEST_MODEL} - ${CTEST_CONFIGURATION_TYPE} --")
ctest_build(BUILD "${CTEST_BINARY_DIRECTORY}")

message("-- Test ${CTEST_MODEL} - ${CTEST_CONFIGURATION_TYPE} --")
ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}" EXCLUDE "${EXCLUDE_RE}" PARALLEL_LEVEL ${N})

message("-- Finished ${CTEST_MODEL} - ${CTEST_CONFIGURATION_TYPE} --")
