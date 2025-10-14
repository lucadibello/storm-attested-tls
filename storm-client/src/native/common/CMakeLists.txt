# CMakeLists.txt for common utilities shared across hosts and clients

add_library(oe_common STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/enclave_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/argument_parser.cpp
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
