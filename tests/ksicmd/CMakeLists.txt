FILE(GLOB cmdlists "*.kl")
add_executable(driver driver.c)
add_dependencies(driver ksicmd)
FOREACH(KL ${cmdlists})
add_test(NAME "ksicmd-${KL}" COMMAND driver "$<TARGET_FILE:ksicmd>" "${KL}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
ENDFOREACH()
