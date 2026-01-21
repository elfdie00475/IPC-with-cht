cmake_minimum_required(VERSION 3.10)
project(zwsystem-IPC)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

#if (NOT BRSTAGING_ROOT)
#    message(FATAL_ERROR "Not set BRSTAGING_ROOT (e.g. -DBRSTAGING_ROOT=/.../output/staging)")
#endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#include_directories("${BRSTAGING_ROOT}/usr/include")
#link_directories("${BRSTAGING_ROOT}/usr/lib")

set(SERVER_SOURCES
    zwsystem_ipc_server.cpp
)
set(SERVER_HEADERS
    zwsystem_ipc_server.h
    zwsystem_ipc_common.h
    zwsystem_ipc_defined.h
)
# client library ==> public C and C++ common
#add_library(zwsystem_ipc_server SHARED ${SERVER_SOURCES})
#target_link_libraries(zwsystem_ipc_server PUBLIC nngipc_handler)

# service bin
add_executable(zwsystem_ipc_service zwsystem_ipc_service.cpp)
target_link_libraries(zwsystem_ipc_service PRIVATE nngipc_handler)
