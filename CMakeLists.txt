cmake_minimum_required(VERSION 3.22...3.30)
project(esp_headless LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF) # hush clangd

add_subdirectory(vendors/spdlog)
add_subdirectory(common)

add_executable(${PROJECT_NAME}
    headless/src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/vendors/asio-1.36.0/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ASIO_STANDALONE
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    spdlog::spdlog_header_only
    esp_common
)