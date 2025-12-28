cmake_minimum_required(VERSION 3.16)
project(alienfx_cli LANGUAGES CXX VERSION 0.0.1)
add_compile_definitions(VERSION="${PROJECT_VERSION}")


# target_compile_features(alienfx_cli PUBLIC cxx_std_23)

file(GLOB_RECURSE CLI_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(alienfx_cli ${CLI_SOURCES})

target_include_directories(alienfx_cli
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(alienfx_cli
    PRIVATE AlienFX_SDK
)

