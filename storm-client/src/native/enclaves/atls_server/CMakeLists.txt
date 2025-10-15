# CMake entry point for the ATLS server enclave bundle (enclave + host)

# Shared metadata so both enclave and host subdirectories can reference
# consistent target names and locations without duplicating literals.
set(ENCLAVE_NAME atls_server)
set(ENCLAVE_BUNDLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ENCLAVE_SIGNED_OUTPUT "${ARTIFACTS_DIR}/enclaves/${ENCLAVE_NAME}/${ENCLAVE_NAME}.signed")

add_subdirectory(enclave)
add_subdirectory(host)
