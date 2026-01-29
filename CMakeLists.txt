cmake_minimum_required(VERSION 3.10)
project(NNG-IPC)

option(BUILD_TEST "In test compile." OFF)

# 設置 C++ 標準
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(NNGIPC_DIR_PATH "set ipc folder path" "/tmp/nngipc")
option(PREBUILD_DIR "set prebuild folder path" "${CMAKE_CURRENT_SOURCE_DIR}/prebuild")
option(BUILD_EXAMPLES "build examples" ON)

if (INCLUDE_OUTPUT_PATH)
else ()
set(INCLUDE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/include CACHE PATH "for install install file path")
endif ()

include_directories(${INCLUDE_OUTPUT_PATH})

#set(PREBUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/prebuild")

if (PREBUILD_DIR)
include_directories("${PREBUILD_DIR}/usr/include")
link_directories("${PREBUILD_DIR}/usr/lib")
else ()
message(FATAL_ERROR "Not set PREBUILD_DIR")
endif ()

add_library(nngipc_handler SHARED
    NngIpcResponseHandler.cpp
    NngIpcRequestHandler.cpp
    NngIpcPublishHandler.cpp
    NngIpcSubscribeHandler.cpp
    NngIpcAioWorker.cpp
    utils.cpp
)
target_link_libraries(nngipc_handler nng)

add_executable(nngipc_pubsub_forwarder NngIpcPubsubForwarder.c)
target_link_libraries(nngipc_pubsub_forwarder nngipc_handler)

if (BUILD_EXAMPLES)
add_subdirectory("demo/pubsub_forwarder")
add_subdirectory("implement/cht_p2pagent")
add_subdirectory("implement/test_pubsub")
add_subdirectory("implement/zwsystemInterface")
endif()

install(TARGETS nngipc_handler
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(TARGETS nngipc_pubsub_forwarder
    RUNTIME DESTINATION bin
)

set(OUTPUT_COLLECTION_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/nngipc.h)
set(OUTPUT_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcAioWorker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcPublishHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcRequestHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcResponseHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcSubscribeHandler.h
)

install(FILES ${OUTPUT_COLLECTION_HEADER} DESTINATION include)
install(FILES ${OUTPUT_HEADER} DESTINATION include/nngipc)

if (INCLUDE_OUTPUT_PATH)
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}")
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}/nngipc")
configure_file(nngipc.h ${INCLUDE_OUTPUT_PATH}/nngipc.h COPYONLY)
configure_file(NngIpcAioWorker.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcAioWorker.h COPYONLY)
configure_file(NngIpcPublishHandler.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcPublishHandler.h COPYONLY)
configure_file(NngIpcRequestHandler.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcRequestHandler.h COPYONLY)
configure_file(NngIpcResponseHandler.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcResponseHandler.h COPYONLY)
configure_file(NngIpcSubscribeHandler.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcSubscribeHandler.h COPYONLY)
endif ()

