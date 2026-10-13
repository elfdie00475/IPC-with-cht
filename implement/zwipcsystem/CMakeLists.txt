cmake_minimum_required(VERSION 3.10)
project(zwsystem-IPC)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (USE_FORWARDING)
add_definitions(-DPUBSUB_USE_FORWARDING=1)
endif ()

set(EVENT_SOURCES
    # ipc event handler
    ${CMAKE_CURRENT_SOURCE_DIR}/zwsystem_ipc_event.cpp

    # ipc repsub handler
    ${CMAKE_CURRENT_SOURCE_DIR}/zwsystem_ipc_sdcardchecker.cpp
)
add_library(zsipc_event SHARED ${EVENT_SOURCES})
target_link_libraries(zsipc_event PUBLIC nngipc_handler)

install(TARGETS zsipc_event
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    INCLUDES DESTINATION include
)

# all ipc event listener
add_executable(zsipc_event_listener zwsystem_ipc_event_listener.cpp)
target_link_libraries(zsipc_event_listener PRIVATE zsipc_event)

install(TARGETS zsipc_event_listener
    RUNTIME DESTINATION bin
)

set(OUTPUT_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/zwsystem_ipc_event_defined.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zwsystem_ipc_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zwsystem_ipc_msg_defined.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zwsystem_ipc_sdcardchecker.h
)

install(FILES ${OUTPUT_HEADER} DESTINATION include/zwsystem_ipc)

if (INCLUDE_OUTPUT_PATH)
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}/zwsystem_ipc")
configure_file(zwsystem_ipc_event_defined.h ${INCLUDE_OUTPUT_PATH}/zwsystem_ipc/zwsystem_ipc_event_defined.h COPYONLY)
configure_file(zwsystem_ipc_event.h ${INCLUDE_OUTPUT_PATH}/zwsystem_ipc/zwsystem_ipc_event.h COPYONLY)
configure_file(zwsystem_ipc_msg_defined.h ${INCLUDE_OUTPUT_PATH}/zwsystem_ipc/zwsystem_ipc_msg_defined.h COPYONLY)
configure_file(zwsystem_ipc_sdcardchecker.h ${INCLUDE_OUTPUT_PATH}/zwsystem_ipc/zwsystem_ipc_sdcardchecker.h COPYONLY)
endif ()

# for 99_PKGS to reference
if (BRSTAGING_ROOT)
set(_staging_includedir "${BRSTAGING_ROOT}/usr/include")
set(_staging_libdir "${BRSTAGING_ROOT}/usr/lib")

# 1) copy include file
file(MAKE_DIRECTORY "${_staging_includedir}/zwsystem_ipc")
configure_file(zwsystem_ipc_event_defined.h ${_staging_includedir}/zwsystem_ipc/zwsystem_ipc_event_defined.h COPYONLY)
configure_file(zwsystem_ipc_event.h ${_staging_includedir}/zwsystem_ipc/zwsystem_ipc_event.h COPYONLY)
configure_file(zwsystem_ipc_msg_defined.h ${_staging_includedir}/zwsystem_ipc/zwsystem_ipc_msg_defined.h COPYONLY)
configure_file(zwsystem_ipc_sdcardchecker.h ${_staging_includedir}/zwsystem_ipc/zwsystem_ipc_sdcardchecker.h COPYONLY)

# 2) copy library file
file(MAKE_DIRECTORY "${_staging_libdir}")
add_custom_command(TARGET zsipc_event POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:zsipc_event>"
          "${_staging_libdir}/$<TARGET_FILE_NAME:zsipc_event>"

  VERBATIM
)
endif ()
