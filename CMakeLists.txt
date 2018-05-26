cmake_minimum_required(VERSION 3.10)

project(blockbar)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Og -ggdb -Wall")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

set(SOURCE
    src/main.c
    src/config.c
    src/render.c
    src/window.c
)

add_executable(blockbar ${SOURCE})

find_package(PkgConfig REQUIRED)
pkg_check_modules(pangocairo REQUIRED pangocairo)

find_library(cairo cairo)
find_library(ujson ujson)
find_library(X11 X11)
find_library(xrandr Xrandr)
target_link_libraries(blockbar ${cairo} ${pangocairo_LIBRARIES} ${ujson} ${X11} ${xrandr})

target_include_directories(blockbar PUBLIC ${pangocairo_INCLUDE_DIRS})
target_compile_options(blockbar PUBLIC ${pangocairo_CFLAGS_OTHER})
