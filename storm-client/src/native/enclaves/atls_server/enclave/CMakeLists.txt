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

# Add include directories for the common headers
target_include_directories(${ENCLAVE_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR} # any header preset in the current directory (const.h, oe_enclave_utility.h)
)

# Link the common enclave library and OpenEnclave networking modules
target_link_libraries(${ENCLAVE_NAME} PRIVATE
    oe_common_enclave # common enclave utilities
    openenclave::oehostsock # OpenEnclave socket support
    openenclave::oehostresolver # OpenEnclave DNS resolver support
)
