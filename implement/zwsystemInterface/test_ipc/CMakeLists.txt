cmake_minimum_required(VERSION 3.16)
project(test_ipc VERSION 0.0.1)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

# 列出所有核心原始檔案
set(CORE_SOURCES
    camera_parameters_manager.cpp
    cht_p2p_camera_control_handler.cpp
    timezone_utils.cpp
)

add_library(cht_p2p_camera_controller SHARED ${CORE_SOURCES})
target_link_libraries(cht_p2p_camera_controller PUBLIC zwsystem_ipc_client)

add_executable(cht_p2p_camera_example_menu cht_p2p_camera_example_menu.cpp)
target_link_libraries(cht_p2p_camera_example_menu cht_p2p_camera_controller)

