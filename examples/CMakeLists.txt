cmake_minimum_required(VERSION 3.20)
project(
        minimal-actor
        VERSION 1.0.0
        DESCRIPTION "minimal actor"
        LANGUAGES CXX)

add_executable(${PROJECT_NAME} minimal_actor.cpp)

# Link against the library target defined in src/CMakeLists.txt
target_link_libraries(${PROJECT_NAME} PRIVATE oxherdcpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
