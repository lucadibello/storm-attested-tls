# Configure the enclave target. Uses shared metadata from the bundle-level
# CMakeLists to avoid hardcoding paths/names here.
add_oe_enclave(
        NAME "${ENCLAVE_NAME}"
        EDL "${ENCLAVE_BUNDLE_DIR}/${ENCLAVE_NAME}.edl"
        CONF "${CMAKE_CURRENT_SOURCE_DIR}/${ENCLAVE_NAME}.conf"
        TRUSTED_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/atls_server_impl.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/oe_enclave_utility.cpp"
            "${CMAKE_SOURCE_DIR}/common/mbedtls_utility.cpp"
            "${CMAKE_SOURCE_DIR}/common/oe_utility.cpp"
)

set(ENCLAVE_GENERATED_TRUSTED_DIR "${OE_LAST_TRUSTED_INCLUDE_DIR}")
set(ENCLAVE_GENERATED_UNTRUSTED_DIR "${OE_LAST_UNTRUSTED_INCLUDE_DIR}")

# Add include directories for the common headers
target_include_directories(${ENCLAVE_NAME} PRIVATE
        "${ENCLAVE_GENERATED_TRUSTED_DIR}"
        ${CMAKE_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR} # any header preset in the current directory (const.h, oe_enclave_utility.h)
)

# Link the common enclave library and OpenEnclave networking modules
target_link_libraries(${ENCLAVE_NAME} PRIVATE
    oe_common_enclave # common enclave utilities
    openenclave::oehostsock # OpenEnclave socket support
    openenclave::oehostresolver # OpenEnclave DNS resolver support
)

# Expose commonly reused metadata to the bundle-level scope so the host can
# link against the generated untrusted stubs without re-discovering paths.
set(ENCLAVE_TARGET "${OE_LAST_ENCLAVE_TARGET}")
set(ENCLAVE_HOST_STUB_TARGET "${OE_LAST_HOST_STUB_TARGET}")
set(ENCLAVE_GEN_UNTRUSTED_TARGET "${OE_LAST_UNTRUSTED_GEN_TARGET}")

set(ENCLAVE_TARGET "${ENCLAVE_TARGET}" PARENT_SCOPE)
set(ENCLAVE_HOST_STUB_TARGET "${ENCLAVE_HOST_STUB_TARGET}" PARENT_SCOPE)
set(ENCLAVE_UNTRUSTED_INCLUDE_DIR "${ENCLAVE_GENERATED_UNTRUSTED_DIR}" PARENT_SCOPE)
set(ENCLAVE_TRUSTED_INCLUDE_DIR "${ENCLAVE_GENERATED_TRUSTED_DIR}" PARENT_SCOPE)
set(ENCLAVE_GEN_UNTRUSTED_TARGET "${ENCLAVE_GEN_UNTRUSTED_TARGET}" PARENT_SCOPE)
