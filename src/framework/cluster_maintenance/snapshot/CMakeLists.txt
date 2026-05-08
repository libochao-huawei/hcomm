set(src_list
    ${CMAKE_CURRENT_SOURCE_DIR}/snapshot_control.cc
)

if(TARGET hcomm)
    target_sources(hcomm PRIVATE
        ${src_list}
    )
endif()