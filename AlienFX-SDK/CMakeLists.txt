cmake_minimum_required(VERSION 3.16)
project(AlienFX_SDK LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
find_package(PkgConfig REQUIRED)

# ----------- LIBUSB ------------

pkg_check_modules(LIBUSB libusb-1.0)
if(NOT LIBUSB_FOUND)
    message(STATUS "libusb-1.0 not found, fetching from GitHub...")
    FetchContent_Declare(libusb
        GIT_REPOSITORY "https://github.com/libusb/libusb.git"
        GIT_TAG "master"
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(libusb)
    set(LIBUSB_LIBRARIES libusb-1.0)
    set(LIBUSB_INCLUDE_DIRS ${libusb_SOURCE_DIR}/libusb)
else()
    message(STATUS "Found system libusb-1.0")
endif()

# ----------- LOGURU ------------

pkg_check_modules(LOGURU loguru)
if(NOT LOGURU_FOUND)
    message(STATUS "loguru not found, fetching from GitHub...")
    FetchContent_Declare(LoguruGitRepo
        GIT_REPOSITORY "https://github.com/emilk/loguru"
        GIT_TAG "master"
        GIT_SHALLOW TRUE
    )
    set(LOGURU_WITH_STREAMS TRUE)
    FetchContent_MakeAvailable(LoguruGitRepo)
    set(LOGURU_LIBRARIES loguru::loguru)
    set(LOGURU_INCLUDE_DIRS ${LoguruGitRepo_SOURCE_DIR})
endif()

# ----------- SDK SOURCES ------------

file(GLOB_RECURSE SDK_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(AlienFX_SDK STATIC ${SDK_SOURCES})

target_include_directories(AlienFX_SDK
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBUSB_INCLUDE_DIRS}
        ${LOGURU_INCLUDE_DIRS}
)

target_link_libraries(AlienFX_SDK
    PUBLIC
        ${LIBUSB_LIBRARIES}
        ${LOGURU_LIBRARIES}
)

