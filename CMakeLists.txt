cmake_minimum_required(VERSION 3.10)
project(NNG-IPC)

option(BUILD_TEST "In test compile." OFF)

# 設置 C++ 標準
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# custom option
# Keep a single path contract for generated headers and endpoint joins: the
# directory always ends in '/', while endpoint names never start with one.
# Use STRING rather than PATH because CMake normalizes PATH values by stripping
# the trailing '/', which would violate the endpoint-join contract.
set(NNGIPC_DIR_PATH "/tmp/nngipc/" CACHE STRING "set ipc folder path")
string(REGEX REPLACE "/+$" "" NNGIPC_DIR_PATH_BASE "${NNGIPC_DIR_PATH}")
set(NNGIPC_DIR_PATH "${NNGIPC_DIR_PATH_BASE}/" CACHE STRING "set ipc folder path" FORCE)
set(PREBUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/prebuild" CACHE PATH "set prebuild folder path" )
option(BUILD_EXAMPLES "build examples" OFF)
option(PUBSUB_USE_FORWARDING "Let Pub/Sub to use proxy to send evnet" OFF)

# Configure the endpoint contract before any example subdirectory is added.
# The generated header is shared by the handler, forwarder and demo.
if (PUBSUB_USE_FORWARDING)
set(NNGIPC_PUBLISH_NAME "pubsub_proxy_front.sock")
set(NNGIPC_SUBSCRIBE_NAME "pubsub_proxy_back.sock")
set(NNGIPC_PROXY_MODE 1)
else()
set(NNGIPC_PUBLISH_NAME "zwsystem_pubsub.ipc")
set(NNGIPC_SUBSCRIBE_NAME "zwsystem_pubsub.ipc")
set(NNGIPC_PROXY_MODE 0)
endif()
set(NNGIPC_PUBSUB_DEFINED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/NngIpcPubsubDefined.h")
set(NNGIPC_PUBSUB_DEFINED_DIR "${CMAKE_CURRENT_BINARY_DIR}")
configure_file(NngIpcPubsubDefined.h.in "${NNGIPC_PUBSUB_DEFINED_HEADER}" @ONLY)

if (INCLUDE_OUTPUT_PATH)
else ()
set(INCLUDE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/include CACHE PATH "for install install file path")
endif ()

include_directories(${INCLUDE_OUTPUT_PATH})

# using this way to above compiler to link the wrong libc.so.6
macro(ADD_PREBUILD_LIB)
add_library(${ARGV0} SHARED IMPORTED GLOBAL)
set_target_properties(${ARGV0} PROPERTIES
    IMPORTED_LOCATION ${PREBUILD_DIR}/lib/lib${ARGV0}.so
    INTERFACE_INCLUDE_DIRECTORIES ${PREBUILD_DIR}/include/)
endmacro()

ADD_PREBUILD_LIB(nng)

add_library(nngipc_handler SHARED
    NngIpcResponseHandler.cpp
    NngIpcRequestHandler.cpp
    NngIpcPublishHandler.cpp
    NngIpcSubscribeHandler.cpp
    NngIpcAioWorker.cpp
    utils.cpp
)
target_link_libraries(nngipc_handler nng)

install(TARGETS nngipc_handler
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

if (BUILD_EXAMPLES)
add_subdirectory("demo/pubsub_forwarder")
endif ()

if (BUILD_TEST)
add_subdirectory("implement/zwsystemInterface")
endif ()

if (PUBSUB_USE_FORWARDING)
# forwarding tool
add_executable(nngipc_pubsub_forwarder NngIpcPubsubForwarder.c)
target_link_libraries(nngipc_pubsub_forwarder nngipc_handler)

install(TARGETS nngipc_pubsub_forwarder
    RUNTIME DESTINATION bin
)
endif()

set(OUTPUT_COLLECTION_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/nngipc.h
)
set(OUTPUT_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcAioWorker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcPath.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcPublishHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcRequestHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcResponseHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcSubscribeHandler.h

    ${NNGIPC_PUBSUB_DEFINED_HEADER}

    ${CMAKE_CURRENT_SOURCE_DIR}/NngIpcCommonTypes.h
)

install(FILES ${OUTPUT_COLLECTION_HEADER} DESTINATION include)
install(FILES ${OUTPUT_HEADER} DESTINATION include/nngipc)

if (INCLUDE_OUTPUT_PATH)
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}")
file(MAKE_DIRECTORY "${INCLUDE_OUTPUT_PATH}/nngipc")
configure_file(nngipc.h ${INCLUDE_OUTPUT_PATH}/nngipc.h COPYONLY)
configure_file(NngIpcAioWorker.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcAioWorker.h COPYONLY)
configure_file(NngIpcPath.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcPath.h COPYONLY)
configure_file(NngIpcPublishHandler.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcPublishHandler.h COPYONLY)
configure_file(NngIpcRequestHandler.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcRequestHandler.h COPYONLY)
configure_file(NngIpcResponseHandler.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcResponseHandler.h COPYONLY)
configure_file(NngIpcSubscribeHandler.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcSubscribeHandler.h COPYONLY)

configure_file(${NNGIPC_PUBSUB_DEFINED_HEADER} ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcPubsubDefined.h COPYONLY)

configure_file(NngIpcCommonTypes.h ${INCLUDE_OUTPUT_PATH}/nngipc/NngIpcCommonTypes.h COPYONLY)
endif ()

# for 99_PKGS to reference
if (BRSTAGING_ROOT)
set(_staging_includedir "${BRSTAGING_ROOT}/usr/include")
set(_staging_libdir "${BRSTAGING_ROOT}/usr/lib")

# 1) copy include file
file(MAKE_DIRECTORY "${_staging_includedir}")
file(MAKE_DIRECTORY "${_staging_includedir}/nngipc")
configure_file(nngipc.h ${_staging_includedir}/nngipc.h COPYONLY)
configure_file(NngIpcAioWorker.h ${_staging_includedir}/nngipc/NngIpcAioWorker.h COPYONLY)
configure_file(NngIpcPath.h ${_staging_includedir}/nngipc/NngIpcPath.h COPYONLY)
configure_file(NngIpcPublishHandler.h ${_staging_includedir}/nngipc/NngIpcPublishHandler.h COPYONLY)
configure_file(NngIpcRequestHandler.h ${_staging_includedir}/nngipc/NngIpcRequestHandler.h COPYONLY)
configure_file(NngIpcResponseHandler.h ${_staging_includedir}/nngipc/NngIpcResponseHandler.h COPYONLY)
configure_file(NngIpcSubscribeHandler.h ${_staging_includedir}/nngipc/NngIpcSubscribeHandler.h COPYONLY)
configure_file(${NNGIPC_PUBSUB_DEFINED_HEADER} ${_staging_includedir}/nngipc/NngIpcPubsubDefined.h COPYONLY)
configure_file(NngIpcCommonTypes.h ${_staging_includedir}/nngipc/NngIpcCommonTypes.h COPYONLY)

# 2) copy library file
file(MAKE_DIRECTORY "${_staging_libdir}")
add_custom_command(TARGET nngipc_handler POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:nngipc_handler>"
          "${_staging_libdir}/$<TARGET_FILE_NAME:nngipc_handler>"

  VERBATIM
)

endif ()
