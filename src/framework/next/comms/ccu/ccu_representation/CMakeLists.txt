file(GLOB_RECURSE CCU_REP_SOURCES "*.cc")

target_sources(hcomm
    PRIVATE
    ${CCU_REP_SOURCES}
)

target_include_directories(hcomm PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
    ${CMAKE_CURRENT_SOURCE_DIR}/context
    ${CMAKE_CURRENT_SOURCE_DIR}/reps
    ${CMAKE_CURRENT_SOURCE_DIR}/reps/arithmetic
    ${CMAKE_CURRENT_SOURCE_DIR}/reps/common
    ${CMAKE_CURRENT_SOURCE_DIR}/reps/control
    ${CMAKE_CURRENT_SOURCE_DIR}/reps/data
    ${CMAKE_CURRENT_SOURCE_DIR}/reps/loop
    ${CMAKE_CURRENT_SOURCE_DIR}/reps/sync
    ${CMAKE_CURRENT_SOURCE_DIR}/reps/translator
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccu_microcode
    ${CMAKE_CURRENT_SOURCE_DIR}/../pub_inc
)