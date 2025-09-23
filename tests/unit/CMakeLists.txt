add_executable(
    unit-tests
    actors/message_actor_tests.cpp actors/finite_state_machine_tests.cpp
    actors/actor_tests.cpp actors/supervisor_tests.cpp)

find_package(GTest REQUIRED)

target_link_libraries(
    unit-tests PRIVATE GTest::gtest GTest::gtest_main
                       oxherdcpp)

include(GoogleTest)

gtest_discover_tests(unit-tests LABELS unit)
