add_executable(integration-tests test.cpp)

find_package(GTest REQUIRED)

target_link_libraries(integration-tests PRIVATE GTest::gtest GTest::gmock
                                                GTest::gtest_main oxherdcpp)

include(GoogleTest)

gtest_discover_tests(integration-tests LABELS integration)
