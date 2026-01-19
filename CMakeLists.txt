cmake_minimum_required(VERSION 3.10)
project(NNG-IPC)

option(BUILD_TEST "In test compile." OFF)

# 設置 C++ 標準
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (INCLUDE_OUTPUT_PATH)
else ()
set(INCLUDE_OUTPUT_PATH ${CMAKE_INSTALL_PREFIX}/include CACHE PATH "for install install file path")
endif ()

include_directories(${INCLUDE_OUTPUT_PATH})

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/out/include")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/out/lib")

add_library(nng_ipc_handler SHARED
    NngIpcResponseHandler.cpp
    NngIpcRequestHandler.cpp
    NngIpcPublishHandler.cpp
    NngIpcSubscribeHandler.cpp
    NngIpcAioWorker.cpp
    utils.cpp
)
target_link_libraries(nng_ipc_handler nng)

add_subdirectory("demo/pubsub_forwarder")
add_subdirectory("implement/cht_p2pagent")
add_subdirectory("implement/test_pubsub")

install(TARGETS nng_ipc_handler
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

if (INCLUDE_OUTPUT_PATH)
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}")
configure_file(NngIpcPublishHandler.h ${INCLUDE_OUTPUT_PATH}/NngIpcPublishHandler.h COPYONLY)
configure_file(NngIpcRequestHandler.h ${INCLUDE_OUTPUT_PATH}/NngIpcRequestHandler.h COPYONLY)
configure_file(NngIpcResponseHandler.h ${INCLUDE_OUTPUT_PATH}/NngIpcResponseHandler.h COPYONLY)
configure_file(NngIpcSubscribeHandler.h ${INCLUDE_OUTPUT_PATH}/NngIpcSubscribeHandler.h COPYONLY)
endif ()

