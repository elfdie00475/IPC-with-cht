cmake_minimum_required(VERSION 3.10)
project(cht_p2pagent)

# 設置 C++ 標準
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (INCLUDE_OUTPUT_PATH)
else ()
set(INCLUDE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/include CACHE PATH "for install install file path")
endif ()

include_directories(${INCLUDE_OUTPUT_PATH})

# 添加可執行文件
add_executable(system_service system_service.cpp)
target_link_libraries(system_service PRIVATE nng_ipc_handler)

add_library(cht_ipc_client SHARED cht_ipc_client.cpp)
target_link_libraries(cht_ipc_client PRIVATE nng_ipc_handler)

add_executable(cht_p2pagent cht_p2pagent.c)
target_link_libraries(cht_p2pagent PRIVATE cht_ipc_client)

install(TARGETS system_service
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(TARGETS cht_ipc_client
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(TARGETS cht_p2pagent
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

set(OUTPUT_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/cht_ipc_client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cht_ipc_common.h
)

install(FILES ${OUTPUT_HEADER} DESTINATION include)

if (INCLUDE_OUTPUT_PATH)
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}")
configure_file("cht_ipc_client.h" ${INCLUDE_OUTPUT_PATH}/cht_ipc_client.h COPYONLY)
configure_file("cht_ipc_common.h" ${INCLUDE_OUTPUT_PATH}/cht_ipc_common.h COPYONLY)
endif ()
