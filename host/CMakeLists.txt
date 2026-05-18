cmake_minimum_required(VERSION 3.22)
project(host LANGUAGES CXX)

add_library(host_lib
    src/host/logging/logger.cpp

    src/host/networking/tcp_connection.cpp
    src/host/networking/tcp_io_state.cpp
    src/host/networking/tcp_server.cpp
)

target_include_directories(host_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/vendors/asio-1.36.0/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(host_lib PUBLIC
    ASIO_STANDALONE
)

target_link_libraries(host_lib PUBLIC
    spdlog::spdlog_header_only
    common
)


add_executable(host src/host/main.cpp)

target_link_libraries(host PRIVATE
    host_lib
)