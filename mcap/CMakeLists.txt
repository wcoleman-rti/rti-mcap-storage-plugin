cmake_minimum_required(VERSION 3.14)
project(RTIStoragePluginMcap::Mcap)

add_library(
    mcap
    STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/mcap.cpp"
)
target_link_libraries(
    mcap
    mcap_interface
)
target_compile_options(
    mcap
    PUBLIC
    -Wall -Wextra -Wpedantic -fPIC
)