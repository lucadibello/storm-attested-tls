# Enclave name: atls_server
add_oe_enclave(
  NAME            atls_server
  EDL             "${CMAKE_CURRENT_SOURCE_DIR}/atls_server.edl"
  CONF            "${CMAKE_CURRENT_SOURCE_DIR}/atls_server.conf"
  TRUSTED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/atls_server_impl.cpp"
)

# Link the common enclave library to get access to shared headers
target_link_libraries(atls_server PRIVATE oe_common_enclave)
