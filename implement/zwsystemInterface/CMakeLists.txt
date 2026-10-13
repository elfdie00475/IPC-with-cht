cmake_minimum_required(VERSION 3.10)
project(zwsystem-IPC)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

#if (NOT BRSTAGING_ROOT)
#    message(FATAL_ERROR "Not set BRSTAGING_ROOT (e.g. -DBRSTAGING_ROOT=/.../output/staging)")
#endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#include_directories("${BRSTAGING_ROOT}/usr/include")
#link_directories("${BRSTAGING_ROOT}/usr/lib")

set(CLIENT_SOURCES
    zwsystem_ipc_client.cpp
)
set(CLIENT_HEADERS
    zwsystem_ipc_client.h
    zwsystem_ipc_common.h
)

add_library(zwsystem_ipc_client SHARED ${CLIENT_SOURCES})
target_link_libraries(zwsystem_ipc_client PUBLIC nngipc_handler)

# client teset bin
add_executable(zwsystem_ipc_client_test zwsystem_ipc_client_test.cpp)
target_link_libraries(zwsystem_ipc_client_test PRIVATE zwsystem_ipc_client nngipc_handler)

set(SERVER_SOURCES
    zwsystem_ipc_service.cpp
)
set(SERVER_HEADERS
    zwsystem_ipc_server.h
    zwsystem_ipc_common.h
    zwsystem_ipc_defined.h
)

# service bin
add_executable(zwsystem_ipc_service ${SERVER_SOURCES})
target_link_libraries(zwsystem_ipc_service PRIVATE nngipc_handler)

install(TARGETS zwsystem_ipc_service
    RUNTIME DESTINATION bin
)

install(TARGETS zwsystem_ipc_client
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES ${CLIENT_HEADERS} DESTINATION include)

if (INCLUDE_OUTPUT_PATH)
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}")
configure_file("zwsystem_ipc_client.h" ${INCLUDE_OUTPUT_PATH}/zwsystem_ipc_client.h COPYONLY)
configure_file("zwsystem_ipc_common.h" ${INCLUDE_OUTPUT_PATH}/zwsystem_ipc_common.h COPYONLY)
endif ()

if (NOT IS_DIRECTORY "${CHT_P2P_AGENT_INCLUDE_DIR}")
    message(FATAL_ERROR
        "BUILD_TEST requires CHT_P2P_AGENT_INCLUDE_DIR, which contains cht_p2p_agent_c.h")
endif()
if (NOT EXISTS "${CHT_P2P_AGENT_LIBRARY}")
    message(FATAL_ERROR
        "BUILD_TEST requires CHT_P2P_AGENT_LIBRARY, the formal CHT P2P Agent SDK library")
endif()

# simulation_cht_p2p_requester 已移除；測試改連結正式 CHT P2P Agent SDK。
add_library(cht_p2p_requester SHARED IMPORTED GLOBAL)
set_target_properties(cht_p2p_requester PROPERTIES
    IMPORTED_LOCATION "${CHT_P2P_AGENT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CHT_P2P_AGENT_INCLUDE_DIR}")
add_subdirectory(test_ipc)
