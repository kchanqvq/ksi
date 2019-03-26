FILE(GLOB cmdlists "*.l")
add_executable(driver driver.c)
add_dependencies(driver ksicmd)
FOREACH(KL ${cmdlists})
get_filename_component(KLNAME ${KL} NAME)
add_test(NAME "ksicmd-${KLNAME}" COMMAND driver "$<TARGET_FILE:ksicmd>" "${KL}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
ENDFOREACH()
