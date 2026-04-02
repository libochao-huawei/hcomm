function(binary_add_version TARGET_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "LOG_FACILITY;LOG_LEVEL" "EXTENDS")

    if(NOT ARG_LOG_FACILITY)
        set(ARG_LOG_FACILITY "LOG_USER")
    endif()
    if(NOT ARG_LOG_LEVEL)
        set(ARG_LOG_LEVEL "LOG_INFO")
    endif()

    get_target_property(TARGET_SO_NAME ${TARGET_NAME} LIBRARY_OUTPUT_NAME)
    if(NOT TARGET_SO_NAME)
        set(TARGET_SO_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}${TARGET_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()
    string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" SAFE_TARGET_NAME "${TARGET_SO_NAME}")

    set(VERSION_LOG_FACILITY "${ARG_LOG_FACILITY}")
    set(VERSION_LOG_LEVEL "${ARG_LOG_LEVEL}")
    set(GEN_FILE "${CMAKE_CURRENT_BINARY_DIR}/version_${TARGET_NAME}.c")
    set(TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    set(COMPILER_INFO "${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")

    list(LENGTH ARG_EXTENDS KV_LEN)
    if(KV_LEN GREATER 0)
        math(EXPR KV_LOOP "${KV_LEN} - 1")
        foreach(IDX RANGE 0 ${KV_LOOP} 2)
            math(EXPR NEXT_IDX "${IDX} + 1")
            if(NEXT_IDX GREATER_EQUAL KV_LEN)
                break()
            endif()

            list(GET ARG_EXTENDS ${IDX} KEY)
            list(GET ARG_EXTENDS ${NEXT_IDX} VAL)
            string(APPEND EXTENSION_LINES "\"  ${KEY}: ${VAL}\",\n\t")
        endforeach()
    endif()

    set(TIMESTAMP_CACHE "${CMAKE_SOURCE_DIR}/build/.hcomm_build_timestamp")
    if(EXISTS "${TIMESTAMP_CACHE}")
        file(READ "${TIMESTAMP_CACHE}" BUILD_TIMESTAMP)
    else()
        string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d%H%M%S" UTC)
        file(WRITE "${TIMESTAMP_CACHE}" "${BUILD_TIMESTAMP}")
    endif()

    configure_file(
        "${HCCP_DIR}/version/version.c.in"
        "${GEN_FILE}"
        @ONLY
    )

    target_sources(${TARGET_NAME} PRIVATE "${GEN_FILE}")

endfunction()