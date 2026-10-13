cmake_minimum_required(VERSION 3.10)
project(test_pubsub)

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
add_executable(test_publisher publisher.cpp)
target_link_libraries(test_publisher PRIVATE nng_ipc_handler)

add_executable(test_subscriber subscriber.cpp)
target_link_libraries(test_subscriber PRIVATE nng_ipc_handler)

add_executable(test_publisher_forward publisher_forward.cpp)
target_link_libraries(test_publisher_forward PRIVATE nng_ipc_handler)

add_executable(test_subscriber_forward subscriber_forward.cpp)
target_link_libraries(test_subscriber_forward PRIVATE nng_ipc_handler)

install(TARGETS test_publisher
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(TARGETS test_subscriber
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(TARGETS test_publisher_forward
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(TARGETS test_subscriber_forward
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
