cmake_minimum_required(VERSION 3.16)
project(AlienFX_Example LANGUAGES CXX)

add_executable(example_app main.cpp)

target_compile_features(example_app PUBLIC cxx_std_23)

target_link_libraries(example_app
    PRIVATE AlienFX_SDK
)

