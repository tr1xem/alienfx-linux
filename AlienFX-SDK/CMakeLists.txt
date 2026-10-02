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

# hidapi's libusb backend unconditionally detaches the kernel driver from
# whatever HID interface it opens (see libusb/hid.c's DETACH_KERNEL_DRIVER),
# reattaching only when the device is closed. On a composite device where an
# AlienFX-family VID also owns a HID *input* interface -- e.g. a Darfon RGB
# keyboard, VID 0x0d62, which is also a plain USB keyboard -- that means
# opening the AlienFX side of the device detaches the OS's own keyboard
# driver from it until the process exits. `alienfx_cli probe` opens every
# detected device and then blocks on an interactive prompt, so on that
# hardware it disables the keyboard and can't be typed at to recover.
#
# hidraw's hid_open_path() is a plain, non-exclusive open() of /dev/hidrawN:
# it never touches the kernel driver, so other processes (including the
# kernel's own usbhid) keep working normally. Default to it; keep libusb as
# an explicit, clearly-warned opt-out for anyone who needs it (e.g. a device
# whose AlienFX interface has no hidraw node at all).
set(ALIENFX_HID_BACKEND
    "hidraw"
    CACHE STRING
          "hidapi backend: hidraw (default, safe) or libusb (detaches kernel drivers)"
)
set_property(CACHE ALIENFX_HID_BACKEND PROPERTY STRINGS hidraw libusb)

if(ALIENFX_HID_BACKEND STREQUAL "hidraw")
  # hidapi's linux/hidraw backend hard-requires libudev
  # (pkg_check_modules(... REQUIRED) in its own CMakeLists.txt); check it
  # here so a missing dependency names the actual distro package instead of
  # failing deep inside hidapi's build.
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ALIENFX_LIBUDEV libudev)
  if(NOT ALIENFX_LIBUDEV_FOUND)
    message(
      FATAL_ERROR
        "ALIENFX_HID_BACKEND=hidraw needs libudev development files "
        "(Arch: systemd-libs; Debian/Ubuntu: libudev-dev; Fedora: "
        "systemd-devel). Or configure with -DALIENFX_HID_BACKEND=libusb "
        "(UNSAFE: detaches kernel drivers from HID interfaces, can disable "
        "keyboards -- see the comment above this option).")
  endif()
  set(HIDAPI_WITH_HIDRAW ON)
  set(HIDAPI_WITH_LIBUSB OFF)
  set(ALIENFX_HIDAPI_TARGET hidapi::hidraw)
elseif(ALIENFX_HID_BACKEND STREQUAL "libusb")
  message(
    WARNING
      "ALIENFX_HID_BACKEND=libusb detaches kernel drivers from HID "
      "interfaces it opens (hidapi's libusb/hid.c DETACH_KERNEL_DRIVER), "
      "reattaching only when the device is closed. On hardware where an "
      "AlienFX-family VID also owns a HID input interface (e.g. some Darfon "
      "RGB keyboards), this can disable that input device for as long as "
      "alienfx_cli is running -- including while `probe` is blocked on an "
      "interactive prompt. Prefer the default hidraw backend.")
  set(HIDAPI_WITH_HIDRAW OFF)
  set(HIDAPI_WITH_LIBUSB ON)
  set(ALIENFX_HIDAPI_TARGET hidapi::libusb)
else()
  message(FATAL_ERROR "ALIENFX_HID_BACKEND must be 'hidraw' or 'libusb', "
                       "got '${ALIENFX_HID_BACKEND}'")
endif()

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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(AlienFX_SDK PRIVATE DEBUG)
endif()
if(ALIENFX_HID_BACKEND STREQUAL "libusb")
  # PUBLIC: alienfx-cli gates probe's interactive behavior on this too, see
  # its CMakeLists.txt-driven build and main.cpp.
  target_compile_definitions(AlienFX_SDK PUBLIC ALIENFX_HID_BACKEND_LIBUSB)
endif()
target_include_directories(AlienFX_SDK
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(AlienFX_SDK PUBLIC usb-1.0 ${ALIENFX_HIDAPI_TARGET}
                                         loguru::loguru
                                         nlohmann_json::nlohmann_json)
