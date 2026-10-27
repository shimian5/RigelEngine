Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(tests
    test_duke1_saves.cpp
    test_camera.cpp
    test_array_view.cpp
    test_duke_script_loader.cpp
    test_elevator.cpp
    test_high_score_list.cpp
    test_json_utils.cpp
    test_letter_collection.cpp
    test_physics_system.cpp
    test_player.cpp
    test_rng.cpp
    test_spike_ball.cpp
    test_string_utils.cpp
    test_timing.cpp
    test_unicycle_bot.cpp
)

target_link_libraries(tests
    PRIVATE
    rigel_core
    Catch2::Catch2WithMain
)

rigel_enable_warnings(tests)

# Ignore warnings emitted from Catch macros
if(CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(tests
        PRIVATE
        -Wno-parentheses
    )
endif()

add_test(all-tests tests)


# Separate process: portable-mode preference lookup is cached globally.
# The fixture supplies generated resources and isolated temporary saves.
add_executable(duke1_frontend_tests
    test_duke1_frontend.cpp
    test_duke1_presentation.cpp
)
target_link_libraries(duke1_frontend_tests PRIVATE rigel_core Catch2::Catch2WithMain)
rigel_enable_warnings(duke1_frontend_tests)
add_test(NAME duke1-frontend-flow COMMAND duke1_frontend_tests)
set_tests_properties(duke1-frontend-flow PROPERTIES TIMEOUT 60)
