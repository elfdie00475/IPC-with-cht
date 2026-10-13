cmake_minimum_required(VERSION 3.16)
project(cht-p2p-requester VERSION 0.0.1)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

# 列出所有核心原始檔案
set(CORE_SOURCES
    cht_p2p_agent_c_stub.cpp
)

add_library(cht_p2p_requester SHARED ${CORE_SOURCES})

if (INCLUDE_OUTPUT_PATH)
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}")
configure_file("cht_p2p_agent_c.h" ${INCLUDE_OUTPUT_PATH}/cht_p2p_agent_c.h COPYONLY)
endif ()
