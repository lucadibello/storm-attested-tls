cmake_minimum_required(VERSION 3.20)
project(oe_host LANGUAGES C CXX)

find_package(OpenEnclave CONFIG REQUIRED)
find_package(JNI REQUIRED)
find_package(spdlog REQUIRED)

# Consume all untrusted stubs and their generators exported by enclaves
get_property(ALL_U_STUBS     GLOBAL PROPERTY OE_ALL_UNTRUSTED_STUBS)
get_property(ALL_U_INC_DIRS  GLOBAL PROPERTY OE_ALL_UNTRUSTED_INC_DIRS)
get_property(ALL_U_GEN_TGTS  GLOBAL PROPERTY OE_ALL_U_GEN_TARGETS)

# Mark generated sources so configure doesn't fail if files
# aren't present yet
if(ALL_U_STUBS)
  set_source_files_properties(${ALL_U_STUBS} PROPERTIES GENERATED TRUE)
endif()

# Ensure ARTIFACTS_DIR is defined (inherited from parent CMakeLists)
if(NOT DEFINED ARTIFACTS_DIR)
  set(ARTIFACTS_DIR "${CMAKE_BINARY_DIR}/artifacts")
endif()

# ---- Shared library for host side (to be packaged with storm-client)
if(BUILD_JNI_HOST)
  add_library(storm_atls_host SHARED
    ${ALL_U_STUBS}
    ${CMAKE_CURRENT_SOURCE_DIR}/host.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jni_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocalls_host.cpp
  )
  if(ALL_U_GEN_TGTS)
    add_dependencies(storm_atls_host ${ALL_U_GEN_TGTS})
  endif()
  target_include_directories(storm_atls_host PRIVATE
    ${ALL_U_INC_DIRS}
    ${OE_INCLUDEDIR}
    ${JNI_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../../target/jni
    ${CMAKE_SOURCE_DIR}/common
  )
  target_compile_features(storm_atls_host PRIVATE c_std_11 cxx_std_17)
  target_compile_definitions(storm_atls_host PRIVATE OE_API_VERSION=2)
  target_link_libraries(storm_atls_host PRIVATE
    openenclave::oehost
    spdlog::spdlog
    oe_common
  )
  if(UNIX AND NOT APPLE)
    target_link_libraries(storm_atls_host PRIVATE dl)
  endif()
  set_target_properties(storm_atls_host PROPERTIES
    OUTPUT_NAME "oe_jni"
    LIBRARY_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}/host"
    RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}/host"
    ARCHIVE_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}/host"
  )
endif()

# ---- Optional small CLI for testing
if(BUILD_HOST_EXE)
  add_executable(oe_cli
    ${ALL_U_STUBS}
    ${CMAKE_CURRENT_SOURCE_DIR}/host.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocalls_host.cpp
  )
  if(ALL_U_GEN_TGTS)
    add_dependencies(oe_cli ${ALL_U_GEN_TGTS})
  endif()
  target_include_directories(oe_cli PRIVATE
    ${ALL_U_INC_DIRS}
    ${OE_INCLUDEDIR}
    ${CMAKE_SOURCE_DIR}/common
  )
  target_link_libraries(oe_cli PRIVATE
    openenclave::oehost
    spdlog::spdlog
    oe_common
  )
  target_compile_features(oe_cli PRIVATE c_std_11 cxx_std_17)

  # Place executable in a predictable folder
  set_target_properties(oe_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}/host"
  )

  # Test configuration options
  set(OE_TEST_PORT "8443" CACHE STRING "Port number for test TLS server")
  set(OE_TEST_SIMULATE OFF CACHE BOOL "Run in simulation mode for testing")
  set(OE_TEST_ENCLAVE_PATH "" CACHE FILEPATH "Path to signed enclave for testing (auto-detected if empty)")

  # Auto-detect the signed enclave path if not specified
  if(OE_TEST_ENCLAVE_PATH STREQUAL "")
    set(OE_TEST_ENCLAVE_PATH "${ARTIFACTS_DIR}/enclaves/atls_server/atls_server.signed")
  endif()

  # Build the command line arguments
  set(TEST_ARGS "")
  if(OE_TEST_SIMULATE)
    list(APPEND TEST_ARGS "--simulate")
  endif()
  list(APPEND TEST_ARGS "${OE_TEST_ENCLAVE_PATH}")
  list(APPEND TEST_ARGS "-port:${OE_TEST_PORT}")

  # Get all signed enclave targets to ensure they're built first
  get_property(ALL_SIGNED_ENCLAVES GLOBAL PROPERTY OE_ALL_SIGNED_TARGETS)

  # `run` target to run the test host with configured flags
  add_custom_target(run
    COMMAND "$<TARGET_FILE:oe_cli>" ${TEST_ARGS}
    DEPENDS oe_cli ${ALL_SIGNED_ENCLAVES}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running test host: oe_cli ${TEST_ARGS}"
    VERBATIM
  )

  # Alternative `test_host` target with same behavior (more descriptive name)
  add_custom_target(test_host
    COMMAND "$<TARGET_FILE:oe_cli>" ${TEST_ARGS}
    DEPENDS oe_cli ${ALL_SIGNED_ENCLAVES}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Testing host with enclave: ${OE_TEST_ENCLAVE_PATH} on port ${OE_TEST_PORT}"
    VERBATIM
  )
endif()

