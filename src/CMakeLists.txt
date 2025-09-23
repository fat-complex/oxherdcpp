# cmake-format: off
CPMAddPackage(
        NAME Boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG boost-1.89.0
        OPTIONS "Boost_USE_STATIC_LIBS ON"
        "Boost_USE_MULTITHREADED ON"
)
# cmake-format: on

find_package(Threads REQUIRED)

add_library(
    oxherdcpp
    actor/actor.cpp
    actor/actor_context.cpp
    actor/actor_ref.cpp
    actor/actor_registry.cpp
    actor/actor_system.cpp
    actor/message/message_dispatcher.cpp
    actor/message/object_pool.cpp
    actor/supervision/supervision_strategy.cpp
    logger/boost_logger.cpp
    logger/logger.cpp)

if(ACTOR_BUILD_SHARED)
    set_target_properties(
        oxherdcpp
        PROPERTIES POSITION_INDEPENDENT_CODE ON
                   CXX_VISIBILITY_PRESET hidden
                   VISIBILITY_INLINES_HIDDEN ON)
endif()

target_include_directories(
    oxherdcpp
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/smart_ptr/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/asio/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/log/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/json/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/phoenix/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/align/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/config/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/core/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/assert/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/throw_exception/include>
           $<BUILD_INTERFACE:${Boost_SOURCE_DIR}/libs/system/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
    oxherdcpp
    PRIVATE
        $<BUILD_INTERFACE:boost_system;boost_filesystem;boost_log;boost_log_setup;boost_json>
        Threads::Threads
    PUBLIC
        $<INSTALL_INTERFACE:Boost::system;Boost::filesystem;Boost::log;Boost::log_setup;Boost::json>
)

target_compile_definitions(
    oxherdcpp PRIVATE $<$<BOOL:${ACTOR_BUILD_SHARED}>:OXHERDCPP_SHARED>)
