cmake_minimum_required(VERSION 3.16)
project(AlienFX_Project LANGUAGES C CXX)

option(ALIENFX_BUILD_CLI "Build alienfx-cli tool" OFF)
option(ALIENFX_BUILD_EXAMPLE "Build Example-App" OFF)

# add_compile_definitions(DEBUG)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Libraries (always)
add_subdirectory(AlienFX-SDK)
add_subdirectory(AlienFan-SDK)

# Optional apps
if(ALIENFX_BUILD_EXAMPLE)
  add_subdirectory(Example-App)
endif()

if(ALIENFX_BUILD_CLI)
  add_subdirectory(alienfx-cli)
endif()
