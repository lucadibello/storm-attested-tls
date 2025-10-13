cmake_minimum_required(VERSION 3.20)
project(oe_host LANGUAGES C CXX)

find_package(OpenEnclave CONFIG REQUIRED)

# Consume all untrusted stubs and their generators exported by enclaves
get_property(ALL_U_STUBS     GLOBAL PROPERTY OE_ALL_UNTRUSTED_STUBS)
get_property(ALL_U_INC_DIRS  GLOBAL PROPERTY OE_ALL_UNTRUSTED_INC_DIRS)
get_property(ALL_U_GEN_TGTS  GLOBAL PROPERTY OE_ALL_U_GEN_TARGETS)

# Mark generated sources so configure doesn’t fail if files
# aren’t present yet
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
  )
  if(ALL_U_GEN_TGTS)
    add_dependencies(storm_atls_host ${ALL_U_GEN_TGTS})
  endif()
  target_include_directories(storm_atls_host PRIVATE ${ALL_U_INC_DIRS} ${OE_INCLUDEDIR})
  target_compile_features(storm_atls_host PRIVATE c_std_11 cxx_std_17)
  target_compile_definitions(storm_atls_host PRIVATE OE_API_VERSION=2)
  target_link_libraries(storm_atls_host PRIVATE openenclave::oehost)
  if(UNIX AND NOT APPLE)
    target_link_libraries(storm_atls_host PRIVATE dl)
  endif()
  set_target_properties(storm_atls_host PROPERTIES
    OUTPUT_NAME "storm_atls_host"
    LIBRARY_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}/host"
    RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}/host"
    ARCHIVE_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}/host"
  )
endif()

# ---- Optional small CLI for testing
if(BUILD_HOST_EXE)
  add_executable(oe_cli
    ${ALL_U_STUBS}
    ${CMAKE_CURRENT_SOURCE_DIR}/host.cpp # test logic (start enclave + make OCALLs)
  )
  if(ALL_U_GEN_TGTS)
    add_dependencies(oe_cli ${ALL_U_GEN_TGTS})
  endif()
  target_include_directories(oe_cli PRIVATE ${ALL_U_INC_DIRS} ${OE_INCLUDEDIR})
  target_link_libraries(oe_cli PRIVATE openenclave::oehost)
  target_compile_features(oe_cli PRIVATE c_std_11 cxx_std_17)

  # Place executable in a predictable folder
  set_target_properties(oe_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ARTIFACTS_DIR}/host"
  )

  # `run` target to run the test host
  add_custom_target(run
    COMMAND "$<TARGET_FILE:oe_cli>"
    DEPENDS oe_cli
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running test host: oe_cli"
  )
endif()
