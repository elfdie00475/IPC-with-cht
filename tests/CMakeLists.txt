cmake_minimum_required(VERSION 3.10)
project(nngipc_worker_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(NNG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../prebuild" CACHE PATH "NNG SDK root")
find_package(Threads REQUIRED)
add_executable(aio_message_ownership
    aio_message_ownership.cpp ../NngIpcAioWorker.cpp)
target_include_directories(aio_message_ownership PRIVATE
    .. "${NNG_ROOT}/include")
target_link_libraries(aio_message_ownership PRIVATE
    "${NNG_ROOT}/lib/libnng.so" Threads::Threads)
target_link_options(aio_message_ownership PRIVATE "-Wl,--wrap=nng_msg_free")
enable_testing()
add_test(NAME aio_message_ownership COMMAND aio_message_ownership)
set_tests_properties(aio_message_ownership PROPERTIES TIMEOUT 30)
