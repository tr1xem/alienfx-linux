cmake_minimum_required(VERSION 3.16)
project(AlienFX_SDK LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(BUILD_SHARED_LIBS OFF)

# ----------- LIBUSB ------------

FetchContent_Declare(
  libusb
  GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
  GIT_TAG main
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(libusb)

# ----------- LOGURU ------------

FetchContent_Declare(
  LoguruGitRepo
  GIT_REPOSITORY https://github.com/emilk/loguru.git
  GIT_TAG master
  GIT_SHALLOW TRUE)
set(LOGURU_WITH_STREAMS ON)
FetchContent_MakeAvailable(LoguruGitRepo)

# ----------- HIDAPI ------------

set(HIDAPI_WITH_LIBUSB ON)
set(HIDAPI_WITH_HIDRAW OFF)

FetchContent_Declare(
  hidapi
  GIT_REPOSITORY https://github.com/libusb/hidapi.git
  GIT_TAG master
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(hidapi)

# target_compile_definitions(${tgt} PRIVATE _GNU_SOURCE)

# ----------- JSON ------------
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG master
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(json)

# ----------- SDK SOURCES ------------

file(GLOB_RECURSE SDK_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(AlienFX_SDK STATIC ${SDK_SOURCES})

target_include_directories(AlienFX_SDK
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(AlienFX_SDK PUBLIC usb-1.0 hidapi::libusb loguru::loguru
                                         nlohmann_json::nlohmann_json)
