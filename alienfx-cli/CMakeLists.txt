cmake_minimum_required(VERSION 3.31)
project(
  alienfx_cli
  LANGUAGES CXX
  VERSION 1.1.0)
add_compile_definitions(VERSION="${PROJECT_VERSION}")

include(FetchContent)

FetchContent_Declare(
  CLI11
  GIT_TAG main
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git)
FetchContent_MakeAvailable(CLI11)
# target_compile_features(alienfx_cli PUBLIC cxx_std_23)

file(GLOB_RECURSE CLI_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(alienfx_cli ${CLI_SOURCES})

target_include_directories(alienfx_cli
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(alienfx_cli PRIVATE AlienFX_SDK AlienFan_SDK CLI11::CLI11)
target_compile_features(alienfx_cli PRIVATE cxx_std_20)
