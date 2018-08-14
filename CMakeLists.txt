cmake_minimum_required(VERSION 3.10)

add_subdirectory(ujson ujson_static EXCLUDE_FROM_ALL)
include_directories(ujson/src)

project(blockbar)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Og -ggdb -Wall")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

set(SOURCE
    src/main.c
    src/config.c
    src/exec.c
    src/render.c
    src/socket.c
    src/tray.c
    src/util.c
    src/window.c
)

add_executable(blockbar ${SOURCE})
add_executable(bbc src/bbc.c)

find_package(PkgConfig REQUIRED)
pkg_check_modules(pangocairo REQUIRED pangocairo)

find_library(cairo cairo)
find_library(X11 X11)
find_library(xrandr Xrandr)
target_link_libraries(blockbar ${cairo} ${pangocairo_LIBRARIES} ${X11} ${xrandr} ujson_static)

target_include_directories(blockbar PUBLIC ${pangocairo_INCLUDE_DIRS})
target_compile_options(blockbar PUBLIC ${pangocairo_CFLAGS_OTHER})

add_custom_command(
    OUTPUT doc/blockbar.1.gz
    DEPENDS doc/blockbar.1
    COMMAND gzip -f < doc/blockbar.1 > doc/blockbar.1.gz)

add_custom_target(
    man ALL DEPENDS doc/blockbar.1.gz)

install(TARGETS blockbar bbc
        RUNTIME DESTINATION bin)

include(GNUInstallDirs)

install(FILES "doc/blockbar.1.gz"
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
        COMPONENT doc)
