cmake_minimum_required(VERSION 3.10)
project(NNG-IPC)

option(BUILD_TEST "In test compile." OFF)

# 設置 C++ 標準
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

if (INCLUDE_OUTPUT_PATH)
else ()
set(INCLUDE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/include CACHE PATH "for install install file path")
endif ()

include_directories(${INCLUDE_OUTPUT_PATH})

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/out/include")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/out/lib")

# 添加可執行文件
add_executable(system_service system_service.cpp)
target_link_libraries(system_service nng_ipc_handler)

add_executable(cht_client client.c)
target_link_libraries(cht_client nng)

add_library(nng_ipc_handler STATIC NngIpcResponseHandler.cpp)
target_link_libraries(nng_ipc_handler nng)

# 連接庫
#target_link_libraries(ipc_service ${NANOMSG_LIBRARIES} ${OPENSSL_LIBRARIES})
#target_link_libraries(ipc_client ${NANOMSG_LIBRARIES} ${OPENSSL_LIBRARIES})
#target_link_libraries(security ${OPENSSL_LIBRARIES})
