find_package(GTest REQUIRED)

add_subdirectory(unit)

if(ACTOR_BUILD_INTEGRATION_TESTS)
    add_subdirectory(integration)
endif()