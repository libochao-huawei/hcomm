# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# JSON Parser for CMake 3.19+
# 解析hcomm_files.json配置并生成安装规则
# -----------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.19)

# 解析JSON配置文件
function(parse_hcomm_json JSON_FILE)
    # 读取JSON文件
    file(READ "${JSON_FILE}" JSON_CONTENT)

    # 替换CMake变量
    string(CONFIGURE "${JSON_CONTENT}" JSON_CONTENT @ONLY)

    # 获取默认值
    string(JSON DEF_PERM GET ${JSON_CONTENT} defaults permission)
    string(JSON DEF_OWNER GET ${JSON_CONTENT} defaults owner)
    string(JSON DEF_PREFIX GET ${JSON_CONTENT} defaults install_prefix)
    string(JSON DEF_TENV GET ${JSON_CONTENT} defaults target_env)

    # 设置全局变量
    set(HCOMM_DEF_PERM "${DEF_PERM}" PARENT_SCOPE)
    set(HCOMM_DEF_OWNER "${DEF_OWNER}" PARENT_SCOPE)
    set(HCOMM_DEF_PREFIX "${DEF_PREFIX}" PARENT_SCOPE)
    set(HCOMM_TARGET_ENV "${DEF_TENV}" PARENT_SCOPE)
    set(HCOMM_JSON_CONTENT "${JSON_CONTENT}" PARENT_SCOPE)
endfunction()

# 解析运行时库
function(parse_runtime_libs JSON_CONTENT OUTPUT_LIST)
    string(JSON BASE_DIR GET ${JSON_CONTENT} runtime_libs base_dir)
    string(JSON FILES_LENGTH LENGTH ${JSON_CONTENT} runtime_libs files)
    math(EXPR FILES_LAST "${FILES_LENGTH} - 1")

    set(INSTALL_LIST "")

    foreach(IDX RANGE ${FILES_LAST})
        string(JSON SRC GET ${JSON_CONTENT} runtime_libs files ${IDX} src)
        string(JSON INSTALL_TYPE ERROR_VARIABLE TYPE_ERR
               GET ${JSON_CONTENT} runtime_libs files ${IDX} install_type)
        if(TYPE_ERR)
            set(INSTALL_TYPE "all")
        endif()

        string(JSON SOFTLINK ERROR_VARIABLE LINK_ERR
               GET ${JSON_CONTENT} runtime_libs files ${IDX} softlink)
        if(LINK_ERR)
            set(SOFTLINK "")
        endif()

        list(APPEND INSTALL_LIST "${BASE_DIR}/${SRC}|${INSTALL_TYPE}|${SOFTLINK}")
    endforeach()

    set(${OUTPUT_LIST} "${INSTALL_LIST}" PARENT_SCOPE)
endfunction()

# 解析头文件
function(parse_headers JSON_CONTENT OUTPUT_LIST)
    # 主头文件
    string(JSON BASE_DIR GET ${JSON_CONTENT} headers base_dir)
    string(JSON FILES_LENGTH LENGTH ${JSON_CONTENT} headers files)
    math(EXPR FILES_LAST "${FILES_LENGTH} - 1")

    set(INSTALL_LIST "")

    foreach(IDX RANGE ${FILES_LAST})
        string(JSON SRC GET ${JSON_CONTENT} headers files ${IDX} src)
        string(JSON DST ERROR_VARIABLE DST_ERR
               GET ${JSON_CONTENT} headers files ${IDX} dst)
        if(DST_ERR)
            set(DST "")
        endif()

        string(JSON INSTALL_TYPE ERROR_VARIABLE TYPE_ERR
               GET ${JSON_CONTENT} headers files ${IDX} install_type)
        if(TYPE_ERR)
            set(INSTALL_TYPE "devel")
        endif()

        string(JSON SOFTLINK ERROR_VARIABLE LINK_ERR
               GET ${JSON_CONTENT} headers files ${IDX} softlink)
        if(LINK_ERR)
            set(SOFTLINK "")
        endif()

        list(APPEND INSTALL_LIST "${BASE_DIR}/${SRC}|${DST}|${INSTALL_TYPE}|${SOFTLINK}")
    endforeach()

    # pkg_inc头文件
    string(JSON PKG_LENGTH LENGTH ${JSON_CONTENT} headers pkg_inc_files)
    if(PKG_LENGTH GREATER 0)
        math(EXPR PKG_LAST "${PKG_LENGTH} - 1")
        foreach(IDX RANGE ${PKG_LAST})
            string(JSON SRC GET ${JSON_CONTENT} headers pkg_inc_files ${IDX} src)
            string(JSON DST ERROR_VARIABLE DST_ERR
                   GET ${JSON_CONTENT} headers pkg_inc_files ${IDX} dst)
            if(DST_ERR)
                set(DST "")
            endif()

            string(JSON INSTALL_TYPE ERROR_VARIABLE TYPE_ERR
                   GET ${JSON_CONTENT} headers pkg_inc_files ${IDX} install_type)
            if(TYPE_ERR)
                set(INSTALL_TYPE "devel")
            endif()

            list(APPEND INSTALL_LIST "${SRC}|${DST}|${INSTALL_TYPE}|")
        endforeach()
    endif()

    set(${OUTPUT_LIST} "${INSTALL_LIST}" PARENT_SCOPE)
endfunction()

# 解析脚本文件
function(parse_scripts JSON_CONTENT OUTPUT_LIST)
    string(JSON BASE_DIR GET ${JSON_CONTENT} scripts base_dir)
    string(JSON INSTALL_DIR GET ${JSON_CONTENT} scripts install_dir)
    string(JSON PERM GET ${JSON_CONTENT} scripts permission)
    string(JSON FILES_LENGTH LENGTH ${JSON_CONTENT} scripts files)
    math(EXPR FILES_LAST "${FILES_LENGTH} - 1")

    set(INSTALL_LIST "")

    foreach(IDX RANGE ${FILES_LAST})
        string(JSON FILE GET ${JSON_CONTENT} scripts files ${IDX})
        list(APPEND INSTALL_LIST "${BASE_DIR}/${FILE}|${INSTALL_DIR}|${PERM}")
    endforeach()

    set(${OUTPUT_LIST} "${INSTALL_LIST}" PARENT_SCOPE)
endfunction()

# 解析配置文件
function(parse_config_files JSON_CONTENT OUTPUT_LIST)
    string(JSON LENGTH LENGTH ${JSON_CONTENT} config_files)
    if(LENGTH EQUAL 0)
        set(${OUTPUT_LIST} "" PARENT_SCOPE)
        return()
    endif()

    math(EXPR LAST "${LENGTH} - 1")
    set(INSTALL_LIST "")

    foreach(IDX RANGE ${LAST})
        string(JSON SRC GET ${JSON_CONTENT} config_files ${IDX} src)
        string(JSON DST GET ${JSON_CONTENT} config_files ${IDX} dst)
        string(JSON PERM ERROR_VARIABLE PERM_ERR
               GET ${JSON_CONTENT} config_files ${IDX} permission)
        if(PERM_ERR)
            set(PERM "440")
        endif()

        list(APPEND INSTALL_LIST "${SRC}|${DST}|${PERM}")
    endforeach()

    set(${OUTPUT_LIST} "${INSTALL_LIST}" PARENT_SCOPE)
endfunction()

# 解析目录结构
function(parse_directories JSON_CONTENT OUTPUT_LIST)
    string(JSON LENGTH LENGTH ${JSON_CONTENT} directories)
    if(LENGTH EQUAL 0)
        set(${OUTPUT_LIST} "" PARENT_SCOPE)
        return()
    endif()

    math(EXPR LAST "${LENGTH} - 1")
    set(INSTALL_LIST "")

    foreach(IDX RANGE ${LAST})
        string(JSON PATH GET ${JSON_CONTENT} directories ${IDX} path)
        string(JSON PERM ERROR_VARIABLE PERM_ERR
               GET ${JSON_CONTENT} directories ${IDX} permission)
        if(PERM_ERR)
            set(PERM "550")
        endif()

        string(JSON SOFTLINK ERROR_VARIABLE LINK_ERR
               GET ${JSON_CONTENT} directories ${IDX} softlink)
        if(LINK_ERR)
            set(SOFTLINK "")
        endif()

        list(APPEND INSTALL_LIST "${PATH}|${PERM}|${SOFTLINK}")
    endforeach()

    set(${OUTPUT_LIST} "${INSTALL_LIST}" PARENT_SCOPE)
endfunction()

# 生成filelist.csv
function(generate_filelist_csv JSON_FILE OUTPUT_CSV)
    # 读取并解析JSON
    file(READ "${JSON_FILE}" JSON_CONTENT)
    string(CONFIGURE "${JSON_CONTENT}" JSON_CONTENT @ONLY)

    # 写入CSV表头
    set(CSV_HEADER "module,operation,relative_path_in_pkg,relative_install_path,")
    string(APPEND CSV_HEADER "is_in_docker,permission,owner:group,install_type,")
    string(APPEND CSV_HEADER "softlink,feature,is_common_path,configurable,hash,")
    string(APPEND CSV_HEADER "block,pkg_inner_softlink,chip,is_dir")

    file(WRITE "${OUTPUT_CSV}" "${CSV_HEADER}\n")

    # 获取默认值
    string(JSON DEF_PERM GET ${JSON_CONTENT} defaults permission)
    string(JSON DEF_OWNER GET ${JSON_CONTENT} defaults owner)
    string(JSON DEF_PREFIX GET ${JSON_CONTENT} defaults install_prefix)

    # 处理运行时库
    string(JSON RUN_LIBS_LENGTH LENGTH ${JSON_CONTENT} runtime_libs files)
    if(RUN_LIBS_LENGTH GREATER 0)
        math(EXPR RUN_LAST "${RUN_LIBS_LENGTH} - 1")
        foreach(IDX RANGE ${RUN_LAST})
            string(JSON SRC GET ${JSON_CONTENT} runtime_libs files ${IDX} src)
            string(JSON TYPE ERROR_VARIABLE TYPE_ERR
                   GET ${JSON_CONTENT} runtime_libs files ${IDX} install_type)
            if(TYPE_ERR)
                set(TYPE "all")
            endif()

            string(JSON SOFTLINK ERROR_VARIABLE LINK_ERR
                   GET ${JSON_CONTENT} runtime_libs files ${IDX} softlink)
            if(LINK_ERR)
                set(SOFTLINK "NA")
            else()
                set(SOFTLINK "${SOFTLINK}")
            endif()

            # 构建CSV行 - 文件实际安装在hcomm/lib64/下
            string(APPEND CSV_LINE "lib64,copy,hcomm/lib64/${SRC},${DEF_PREFIX}/lib64/${SRC},")
            string(APPEND CSV_LINE "TRUE,${DEF_PERM},${DEF_OWNER},${TYPE},${SOFTLINK},")
            string(APPEND CSV_LINE "all,N,FALSE,NA,CommLib,NA,all,FALSE\n")

            file(APPEND "${OUTPUT_CSV}" "${CSV_LINE}")
            set(CSV_LINE "")
        endforeach()
    endif()

    # 处理object_files
    string(JSON OBJ_LENGTH ERROR_VARIABLE OBJ_ERR LENGTH ${JSON_CONTENT} runtime_libs object_files)
    if(NOT OBJ_ERR AND OBJ_LENGTH GREATER 0)
        math(EXPR OBJ_LAST "${OBJ_LENGTH} - 1")
        foreach(IDX RANGE ${OBJ_LAST})
            string(JSON OBJ GET ${JSON_CONTENT} runtime_libs object_files ${IDX})

            string(APPEND CSV_LINE "lib64,copy,hcomm/lib64/${OBJ},${DEF_PREFIX}/lib64/${OBJ},")
            string(APPEND CSV_LINE "TRUE,${DEF_PERM},${DEF_OWNER},all,NA,")
            string(APPEND CSV_LINE "all,N,FALSE,NA,CommLib,NA,all,FALSE\n")

            file(APPEND "${OUTPUT_CSV}" "${CSV_LINE}")
            set(CSV_LINE "")
        endforeach()
    endif()

    # 处理头文件
    string(JSON HDR_LENGTH LENGTH ${JSON_CONTENT} headers files)
    if(HDR_LENGTH GREATER 0)
        math(EXPR HDR_LAST "${HDR_LENGTH} - 1")
        foreach(IDX RANGE ${HDR_LAST})
            string(JSON SRC GET ${JSON_CONTENT} headers files ${IDX} src)
            string(JSON DST ERROR_VARIABLE DST_ERR
                   GET ${JSON_CONTENT} headers files ${IDX} dst)
            if(DST_ERR)
                set(DST "include")
            endif()

            string(JSON TYPE ERROR_VARIABLE TYPE_ERR
                   GET ${JSON_CONTENT} headers files ${IDX} install_type)
            if(TYPE_ERR)
                set(TYPE "devel")
            endif()

            # 提取文件名
            get_filename_component(BASENAME "${SRC}" NAME)

            string(APPEND CSV_LINE "include,copy,hcomm/${DST}/${BASENAME},${DEF_PREFIX}/${DST}/${BASENAME},")
            string(APPEND CSV_LINE "FALSE,${DEF_PERM},${DEF_OWNER},${TYPE},NA,")
            string(APPEND CSV_LINE "all,N,FALSE,NA,CommLib,NA,all,FALSE\n")

            file(APPEND "${OUTPUT_CSV}" "${CSV_LINE}")
            set(CSV_LINE "")
        endforeach()
    endif()

    # 处理脚本文件
    string(JSON BASE_DIR GET ${JSON_CONTENT} scripts base_dir)
    string(JSON INSTALL_DIR GET ${JSON_CONTENT} scripts install_dir)
    string(JSON SCRIPT_PERM GET ${JSON_CONTENT} scripts permission)
    string(JSON SCRIPT_LENGTH LENGTH ${JSON_CONTENT} scripts files)

    if(SCRIPT_LENGTH GREATER 0)
        math(EXPR SCRIPT_LAST "${SCRIPT_LENGTH} - 1")
        foreach(IDX RANGE ${SCRIPT_LAST})
            string(JSON SCRIPT_FILE GET ${JSON_CONTENT} scripts files ${IDX})

            string(APPEND CSV_LINE "script,copy,${INSTALL_DIR}/${SCRIPT_FILE},${INSTALL_DIR}/${SCRIPT_FILE},")
            string(APPEND CSV_LINE "TRUE,${SCRIPT_PERM},${DEF_OWNER},all,NA,")
            string(APPEND CSV_LINE "all,N,FALSE,NA,CommLib,NA,all,FALSE\n")

            file(APPEND "${OUTPUT_CSV}" "${CSV_LINE}")
            set(CSV_LINE "")
        endforeach()
    endif()

    # 处理配置文件
    string(JSON CONF_LENGTH LENGTH ${JSON_CONTENT} config_files)
    if(CONF_LENGTH GREATER 0)
        math(EXPR CONF_LAST "${CONF_LENGTH} - 1")
        foreach(IDX RANGE ${CONF_LAST})
            string(JSON SRC GET ${JSON_CONTENT} config_files ${IDX} src)
            string(JSON DST GET ${JSON_CONTENT} config_files ${IDX} dst)
            string(JSON PERM ERROR_VARIABLE PERM_ERR
                   GET ${JSON_CONTENT} config_files ${IDX} permission)
            if(PERM_ERR)
                set(PERM "440")
            endif()

            # 提取文件名
            get_filename_component(BASENAME "${DST}" NAME)

            string(APPEND CSV_LINE "config,copy,${DST},${BASENAME},")
            string(APPEND CSV_LINE "TRUE,${PERM},${DEF_OWNER},all,NA,")
            string(APPEND CSV_LINE "all,N,FALSE,NA,CommLib,NA,all,FALSE\n")

            file(APPEND "${OUTPUT_CSV}" "${CSV_LINE}")
            set(CSV_LINE "")
        endforeach()
    endif()

    # 处理目录
    string(JSON DIR_LENGTH LENGTH ${JSON_CONTENT} directories)
    if(DIR_LENGTH GREATER 0)
        math(EXPR DIR_LAST "${DIR_LENGTH} - 1")
        foreach(IDX RANGE ${DIR_LAST})
            string(JSON DIR_PATH GET ${JSON_CONTENT} directories ${IDX} path)
            string(JSON DIR_PERM ERROR_VARIABLE PERM_ERR
                   GET ${JSON_CONTENT} directories ${IDX} permission)
            if(PERM_ERR)
                set(DIR_PERM "550")
            endif()

            string(APPEND CSV_LINE "dir,mkdir,NA,${DIR_PATH},")
            string(APPEND CSV_LINE "TRUE,${DIR_PERM},${DEF_OWNER},all,NA,")
            string(APPEND CSV_LINE "all,N,FALSE,NA,CommLib,NA,all,TRUE\n")

            file(APPEND "${OUTPUT_CSV}" "${CSV_LINE}")
            set(CSV_LINE "")
        endforeach()
    endif()

    message(STATUS "Generated filelist.csv: ${OUTPUT_CSV}")
endfunction()

# 生成scene.info
function(generate_scene_info OUTPUT_FILE)
    file(WRITE "${OUTPUT_FILE}" "os=@CMAKE_SYSTEM_NAME@\n")
    file(APPEND "${OUTPUT_FILE}" "arch=@CMAKE_SYSTEM_PROCESSOR@\n")
    message(STATUS "Generated scene.info: ${OUTPUT_FILE}")
endfunction()

# 生成hcomm_version.h
function(generate_version_header OUTPUT_FILE)
    file(WRITE "${OUTPUT_FILE}" "#ifndef HCOMM_VERSION\n")
    file(APPEND "${OUTPUT_FILE}" "#define HCOMM_VERSION\n")
    file(APPEND "${OUTPUT_FILE}" "#define HCOMM_VERSION @PROJECT_VERSION@\n")
    file(APPEND "${OUTPUT_FILE}" "#define HCOMM_TIMESTAMP \"@BUILD_TIMESTAMP@\"\n")
    file(APPEND "${OUTPUT_FILE}" "#endif /* HCOMM_VERSION */\n")
    message(STATUS "Generated hcomm_version.h: ${OUTPUT_FILE}")
endfunction()

# 生成version.info
function(generate_version_info OUTPUT_FILE)
    file(WRITE "${OUTPUT_FILE}" "Version=@PROJECT_VERSION@\n")
    message(STATUS "Generated version.info: ${OUTPUT_FILE}")
endfunction()
