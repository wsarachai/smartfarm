cmake_minimum_required(VERSION 3.10)   # ships on Ubuntu 18.04 / L4T (3.10.2)
project(jetson-ctrl CXX)

# NOTE: 3.10.2 is a hard floor -- it is what JetPack 4.x has, and the daemon is
# built on the target. Anything newer must not creep in. Banned here:
#   target_link_directories()  3.13   -> link_directories() before the target
#   cmake --build -j <N>       3.12   -> cmake --build build -- -j$(nproc)
#   cmake --install            3.15   -> make -C build install
#   ctest --test-dir           3.20   -> cd build && ctest

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

# --- Dependencies ------------------------------------------------------------
# libcurl  (HTTP client)   : apt install libcurl4-openssl-dev
# libgpiod (GPIO)          : apt install libgpiod-dev
# pthreads (sampling thread)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL   REQUIRED libcurl)
pkg_check_modules(GPIOD  REQUIRED libgpiod)

# nlohmann/json: prefer the vendored single header (see third_party/nlohmann/),
# fall back to a system package if one is installed.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/json.hpp")
  add_library(nlohmann_json INTERFACE)
  target_include_directories(nlohmann_json INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
  set(JSON_TARGET nlohmann_json)
else()
  find_package(nlohmann_json 3.2.0 REQUIRED)
  set(JSON_TARGET nlohmann_json::nlohmann_json)
endif()

# --- Target ------------------------------------------------------------------
# Directory-scoped, and must precede the target it applies to: the per-target
# form (target_link_directories) is CMake 3.13+, which JetPack 4.x does not have.
link_directories(${CURL_LIBRARY_DIRS} ${GPIOD_LIBRARY_DIRS})

add_executable(jetson-ctrl
  src/main.cpp
  src/config.cpp
  src/thermal.cpp
  src/dht22.cpp
  src/fan.cpp
  src/control.cpp
  src/net.cpp
)

target_include_directories(jetson-ctrl PRIVATE
  ${CURL_INCLUDE_DIRS} ${GPIOD_INCLUDE_DIRS})
target_link_libraries(jetson-ctrl PRIVATE
  ${JSON_TARGET}
  Threads::Threads
  ${CURL_LIBRARIES}
  ${GPIOD_LIBRARIES})

# --- Tests -------------------------------------------------------------------
# The control law is pure (depends only on the Config struct), so its tests link
# against src/control.cpp alone — no libcurl/gpiod/json — and run on any host.
#   cmake -S . -B build && cmake --build build && (cd build && ctest)
enable_testing()
add_executable(test_control tests/test_control.cpp src/control.cpp)
target_include_directories(test_control PRIVATE src)
add_test(NAME control COMMAND test_control)

# --- Install -----------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS jetson-ctrl RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(FILES systemd/jetson-ctrl.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system)
# config.example.json is installed only as a reference; the LIVE config lives at
# /etc/jetson-ctrl/config.json and is NOT overwritten by install (see README).
install(FILES config.example.json DESTINATION ${CMAKE_INSTALL_DOCDIR})
