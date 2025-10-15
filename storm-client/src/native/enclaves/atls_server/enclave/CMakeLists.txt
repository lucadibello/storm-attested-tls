# Enclave name: atls_server
# EDL "${CMAKE_SOURCE_DIR}/enclaves/atls_server/atls_server.edl"
add_oe_enclave(
        NAME atls_server
        EDL "${CMAKE_CURRENT_SOURCE_DIR}/../atls_server.edl"
        CONF "${CMAKE_CURRENT_SOURCE_DIR}/atls_server.conf"
        TRUSTED_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/atls_server_impl.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/oe_enclave_utility.cpp"
            "${CMAKE_SOURCE_DIR}/common/mbedtls_utility.cpp"
            "${CMAKE_SOURCE_DIR}/common/oe_utility.cpp"
)

# Add include directories for the common headers
target_include_directories(atls_server PRIVATE
        ${CMAKE_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR} # any header preset in the current directory (const.h, oe_enclave_utility.h)
)

# Link the common enclave library and OpenEnclave networking modules
target_link_libraries(atls_server PRIVATE
    oe_common_enclave # common enclave utilities
    openenclave::oehostsock # OpenEnclave socket support
    openenclave::oehostresolver # OpenEnclave DNS resolver support
)