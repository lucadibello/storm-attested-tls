# CMakeLists.txt for common utilities shared across hosts and clients

# Host-side common library
add_library(oe_common STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/tcp_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/argument_parser.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls_utility.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls_utility.h

        ${CMAKE_CURRENT_SOURCE_DIR}/oe_utility.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/oe_utility.h

        ${CMAKE_CURRENT_SOURCE_DIR}/enclave_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/enclave_manager.h

        ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls_utility.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls_utility.h
)

target_include_directories(oe_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OE_INCLUDEDIR}
)

target_link_libraries(oe_common PUBLIC
    openenclave::oehost
)

target_compile_features(oe_common PUBLIC cxx_std_17)

# Enable Position Independent Code for linking into shared libraries
set_target_properties(oe_common PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Enclave-side common library (header-only or enclave-compatible sources)
# This library is for use within enclaves - it only includes files that can
# be compiled for the enclave environment (no host-specific dependencies)
add_library(oe_common_enclave INTERFACE)

target_include_directories(oe_common_enclave INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(oe_common_enclave INTERFACE cxx_std_17)