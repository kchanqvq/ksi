cmake_minimum_required(VERSION 3.13)
add_executable(config_gen configure.c)
add_custom_command(
    OUTPUT  ${CMAKE_BINARY_DIR}/_config.h
    COMMAND config_gen ${CMAKE_BINARY_DIR}/_config.h
)
add_custom_target(generate_config DEPENDS ${CMAKE_BINARY_DIR}/_config.h)
