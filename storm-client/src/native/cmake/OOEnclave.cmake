# Helper to locate Open Enclave tools as executable paths usable by custom commands
# Prefer imported targets if available, otherwise fall back to PATH lookups.
if (NOT DEFINED OE_EDGER8R)
    if (TARGET openenclave::oeedger8r)
        set(OE_EDGER8R $<TARGET_FILE:openenclave::oeedger8r>)
    else ()
        find_program(OE_EDGER8R NAMES oeedger8r REQUIRED)
    endif ()
endif ()
if (NOT DEFINED OE_SIGN)
    if (TARGET openenclave::oesign)
        set(OE_SIGN $<TARGET_FILE:openenclave::oesign>)
    else ()
        find_program(OE_SIGN NAMES oesign REQUIRED)
    endif ()
endif ()

# add_oe_enclave(
#   NAME <name>
#   EDL  <path/to/name.edl>
#   CONF <path/to/name.conf>
#   TRUSTED_SOURCES <file1> [file2 ...]
# )
#
# Generates:
#   - Trusted stubs:  <build>/generated/<name>/<name>_t.{h,c}
#   - Untrusted stubs:<build>/generated/<name>/<name>_u.{h,c}
#   - Enclave target: <name>
#   - Signed file:    <binary_dir>/artifacts/enclaves/<name>/<name>.signed
#
# Aggregates untrusted stubs into GLOBAL props:
#   OE_ALL_UNTRUSTED_STUBS, OE_ALL_UNTRUSTED_INC_DIRS, OE_ALL_SIGNED_TARGETS

function(add_oe_enclave)
    cmake_parse_arguments(AOE "" "NAME;EDL;CONF" "TRUSTED_SOURCES" ${ARGN})

    if (NOT AOE_NAME OR NOT AOE_EDL OR NOT AOE_CONF)
        message(FATAL_ERROR "add_oe_enclave requires NAME, EDL, CONF")
    endif ()

    set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/${AOE_NAME}")
    set(_gen_trusted "${_gen_dir}/trusted")
    set(_gen_untrusted "${_gen_dir}/untrusted")
    file(MAKE_DIRECTORY "${_gen_trusted}" "${_gen_untrusted}")

    # Trusted stubs
    add_custom_command(
            OUTPUT "${_gen_trusted}/${AOE_NAME}_t.h" "${_gen_trusted}/${AOE_NAME}_t.c" "${_gen_trusted}/${AOE_NAME}_args.h"
            DEPENDS "${AOE_EDL}"
            COMMAND ${OE_EDGER8R} --trusted "${AOE_EDL}"
            --search-path "${OE_INCLUDEDIR}" --search-path "${OE_INCLUDEDIR}/openenclave/edl/sgx"
            --trusted-dir "${_gen_trusted}"
            COMMENT "oeedger8r (trusted) ${AOE_EDL}"
            VERBATIM)

    # Untrusted stubs
    add_custom_command(
            OUTPUT "${_gen_untrusted}/${AOE_NAME}_u.h" "${_gen_untrusted}/${AOE_NAME}_u.c" "${_gen_untrusted}/${AOE_NAME}_args.h"
            DEPENDS "${AOE_EDL}"
            COMMAND ${OE_EDGER8R} --untrusted "${AOE_EDL}"
            --search-path "${OE_INCLUDEDIR}" --search-path "${OE_INCLUDEDIR}/openenclave/edl/sgx"
            --untrusted-dir "${_gen_untrusted}"
            COMMENT "oeedger8r (untrusted) ${AOE_EDL}"
            VERBATIM)

    # generator target for untrusted stubs (needed to let the host generate these files at runtime!)
    add_custom_target(gen_u_${AOE_NAME}
            DEPENDS
            "${_gen_untrusted}/${AOE_NAME}_u.c"
            "${_gen_untrusted}/${AOE_NAME}_u.h"
            "${_gen_untrusted}/${AOE_NAME}_args.h"
    )

    # Export generator + paths globally for host
    set_property(GLOBAL APPEND PROPERTY OE_ALL_U_GEN_TARGETS gen_u_${AOE_NAME})

    get_property(_acc_u GLOBAL PROPERTY OE_ALL_UNTRUSTED_STUBS)
    list(APPEND _acc_u "${_gen_untrusted}/${AOE_NAME}_u.c")
    set_property(GLOBAL PROPERTY OE_ALL_UNTRUSTED_STUBS "${_acc_u}")

    get_property(_acc_inc GLOBAL PROPERTY OE_ALL_UNTRUSTED_INC_DIRS)
    list(APPEND _acc_inc "${_gen_untrusted}")
    list(REMOVE_DUPLICATES _acc_inc)
    set_property(GLOBAL PROPERTY OE_ALL_UNTRUSTED_INC_DIRS "${_acc_inc}")

    # Enclave target
    add_executable(${AOE_NAME}
            "${_gen_trusted}/${AOE_NAME}_t.c"
            ${AOE_TRUSTED_SOURCES}
    )

    if (WIN32)
        maybe_build_using_clangw(${AOE_NAME})
    endif ()

    target_compile_definitions(${AOE_NAME} PUBLIC OE_API_VERSION=2)
    target_compile_features(${AOE_NAME} PRIVATE c_std_11 cxx_std_17)
    target_include_directories(${AOE_NAME} PRIVATE "${_gen_trusted}")

    if (LVI_MITIGATION MATCHES ControlFlow)
        apply_lvi_mitigation(${AOE_NAME})
        target_link_libraries(${AOE_NAME} PRIVATE
                openenclave::oeenclave-lvi-cfg
                openenclave::oecrypto${OE_CRYPTO_LIB}-lvi-cfg
                openenclave::oelibcxx-lvi-cfg)
    else ()
        target_link_libraries(${AOE_NAME} PRIVATE
                openenclave::oeenclave
                openenclave::oecrypto${OE_CRYPTO_LIB}
                openenclave::oelibcxx)
    endif ()

    # Ensure signing keys are ready before building/signing the enclave
    add_dependencies(${AOE_NAME} oe_sign_keys)
    # Signing
    if (NOT DEFINED ARTIFACTS_DIR)
        set(ARTIFACTS_DIR "${CMAKE_BINARY_DIR}/artifacts")
    endif ()
    set(_signed_dir "${ARTIFACTS_DIR}/enclaves/${AOE_NAME}")
    set(_signed_path "${_signed_dir}/${AOE_NAME}.signed")

    add_custom_command(
            TARGET ${AOE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_signed_dir}"
            COMMAND openenclave::oesign sign
            -e $<TARGET_FILE:${AOE_NAME}>
            -c "${AOE_CONF}"
            -k "${OE_SIGN_KEY}"
            -o "${_signed_path}"
            BYPRODUCTS "${_signed_path}"
            COMMENT "Signing enclave '${AOE_NAME}' with ${OE_SIGN_KEY} -> ${_signed_path}"
            VERBATIM
    )

    # Export signed target list (for sign_all convenience)
    set_property(GLOBAL APPEND PROPERTY OE_ALL_SIGNED_TARGETS ${AOE_NAME})
endfunction()
