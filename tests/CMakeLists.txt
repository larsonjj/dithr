# --- Input tests ---
add_executable(test_input test_input.c)
target_link_libraries(test_input PRIVATE mvngin_core)
if (MSVC)
    target_compile_options(test_input PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_input POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_input> $<TARGET_FILE_DIR:test_input>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_input COMMAND test_input)

# --- Cart tests ---
add_executable(test_cart test_cart.c)
target_link_libraries(test_cart PRIVATE mvngin_core)
if (MSVC)
    target_compile_options(test_cart PRIVATE /wd4244)
endif()
if (WIN32)
    add_custom_command(TARGET test_cart POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_cart> $<TARGET_FILE_DIR:test_cart>
        COMMAND_EXPAND_LISTS
    )
endif()
add_test(NAME test_cart COMMAND test_cart)
