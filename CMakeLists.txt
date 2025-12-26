cmake_minimum_required(VERSION 3.16)
project(alienfx)
set(APP_NAME alienfx)


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
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(libusb)
    set(LIBUSB_LIBRARIES libusb-1.0)
    set(LIBUSB_INCLUDE_DIRS ${libusb_SOURCE_DIR}/libusb)
    set(LIBUSB_CFLAGS_OTHER "")
else()
    message(STATUS "Found system libusb-1.0: ${LIBUSB_LIBRARIES}")
    set(LIBUSB_LIBRARIES ${LIBUSB_LIBRARIES})
    set(LIBUSB_INCLUDE_DIRS ${LIBUSB_INCLUDE_DIRS})
    set(LIBUSB_CFLAGS_OTHER ${LIBUSB_CFLAGS_OTHER})
endif()

# ----------- Loguru ------------
pkg_check_modules(LOGURU loguru)
if(NOT LOGURU_FOUND)
    message(STATUS "loguru not found, fetching from GitHub...")
    FetchContent_Declare(LoguruGitRepo
        GIT_REPOSITORY "https://github.com/emilk/loguru"
        GIT_TAG        "master"
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    set(LOGURU_WITH_STREAMS TRUE)
    FetchContent_MakeAvailable(LoguruGitRepo)
    set(LOGURU_LIBRARIES loguru::loguru)
    set(LOGURU_INCLUDE_DIRS ${LoguruGitRepo_SOURCE_DIR})
else()
    message(STATUS "Found system loguru: ${LOGURU_LIBRARIES}")
    set(LOGURU_LIBRARIES ${LOGURU_LIBRARIES})
    set(LOGURU_INCLUDE_DIRS ${LOGURU_INCLUDE_DIRS})
endif()


file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(${APP_NAME} ${SOURCES})

target_include_directories(${APP_NAME} PRIVATE
    include
    ${LIBUSB_INCLUDE_DIRS}
    ${LOGURU_INCLUDE_DIRS}
)
target_link_libraries(${APP_NAME}
    ${LIBUSB_LIBRARIES}
    ${LOGURU_LIBRARIES}

)
