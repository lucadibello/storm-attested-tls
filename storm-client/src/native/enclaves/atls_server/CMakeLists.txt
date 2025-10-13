
# Enclave name: atls_server
add_oe_enclave(
  NAME            atls_server
  EDL             "${CMAKE_CURRENT_SOURCE_DIR}/atls_server.edl"
  CONF            "${CMAKE_CURRENT_SOURCE_DIR}/atls_server.conf"
  TRUSTED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/atls_server_impl.cpp"
)
