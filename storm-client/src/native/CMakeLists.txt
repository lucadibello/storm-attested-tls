cmake_minimum_required(VERSION 3.20)

# --- Toolchain / compiler setup ---
if(LVI_MITIGATION MATCHES ControlFlow)
  find_package(OpenEnclave-LVI-Mitigation CONFIG REQUIRED)
else()
  if(UNIX)
    if(NOT DEFINED ENV{CC} AND NOT DEFINED CMAKE_C_COMPILER)
      find_program(CMAKE_C_COMPILER clang-16 clang-15 clang-14
        clang-13 clang-12 clang-11 clang)
    endif()
    if(NOT DEFINED ENV{CXX} AND NOT DEFINED CMAKE_CXX_COMPILER)
      find_program(CMAKE_CXX_COMPILER clang++-16 clang++-15 clang++-14
        clang++-13 clang++-12 clang++-11 clang++)
    endif()
  endif()
endif()

project("OE - Attested TLS Server" LANGUAGES C CXX)

# --- Open Enclave ---
find_package(OpenEnclave CONFIG REQUIRED)
if(NOT DEFINED OE_INCLUDEDIR) # fallback if not present in older systems
  set(OE_INCLUDEDIR "${OpenEnclave_INCLUDE_DIRS}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# Crypto lib same as sample convention (mbedtls/openssl)
set(OE_CRYPTO_LIB mbedtls
    CACHE STRING "Crypto lib for enclaves (mbedtls|openssl)")

# additional config
option(BUILD_JNI_HOST "Build shared JNI host library" ON)
option(BUILD_HOST_EXE "Build a small CLI host for tests" OFF)

# Artifacts root
set(ARTIFACTS_DIR "${CMAKE_BINARY_DIR}/artifacts")
file(MAKE_DIRECTORY "${ARTIFACTS_DIR}")

# Global bucket for all generated untrusted stubs (*.u.c from all enclaves)
# NOTE: the host will link against these files
set_property(GLOBAL PROPERTY OE_ALL_UNTRUSTED_STUBS "")
set_property(GLOBAL PROPERTY OE_ALL_UNTRUSTED_INC_DIRS "")
set_property(GLOBAL PROPERTY OE_ALL_U_GEN_TARGETS "")
set_property(GLOBAL PROPERTY OE_ALL_SIGNED_TARGETS "")

# --- Signing key management ---
# NOTE: If you already have a PEM, pass -DOE_SIGN_KEY="/abs/path/to/private.pem"
set(OE_SIGN_KEY ""
  CACHE FILEPATH
  "Path to enclave signing private key (PEM). If empty, one is generated."
)

if(OE_SIGN_KEY STREQUAL "")
  # Generate keys once into build/signing/
  set(SIGN_DIR "${CMAKE_BINARY_DIR}/signing")
  set(GEN_PRIV "${SIGN_DIR}/private.pem")
  set(GEN_PUB  "${SIGN_DIR}/public.pem")
  file(MAKE_DIRECTORY "${SIGN_DIR}")

  add_custom_command(
    OUTPUT "${GEN_PRIV}" "${GEN_PUB}"
    COMMAND ${CMAKE_COMMAND} -E
      echo "Generating enclave signing keypair in ${SIGN_DIR}"
    COMMAND openssl genrsa -out "${GEN_PRIV}" -3 3072
    COMMAND openssl rsa -in "${GEN_PRIV}" -pubout -out "${GEN_PUB}"
    COMMENT "OpenSSL: generating enclave signing keypair"
    VERBATIM)

  add_custom_target(oe_sign_keys ALL
    DEPENDS "${GEN_PRIV}" "${GEN_PUB}")

  # Export the generated private key path via a GLOBAL property
  set(OE_SIGN_KEY "${GEN_PRIV}")
else()
  # User-supplied key: create a phony target so enclaves can depend on it
  add_custom_target(oe_sign_keys)
endif()

# --- Additional helper modules --- Make the key path discoverable by subdirs
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(OOEnclave)  # provides add_oe_enclave()

# --- Enclave (hosting the attested TLS server) ---
add_subdirectory(enclaves/atls_server)

# --- Host (for testing / JNI lib) ---
add_subdirectory(host)

# --- utility/QoL targets

# Aggregate convenience target
add_custom_target(sign_all DEPENDS ${OE_ALL_SIGNED_TARGETS})
