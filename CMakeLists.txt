cmake_minimum_required(VERSION 3.10)

include(GNUInstallDirs)

include_directories(include/blockbar)

project(blockbar)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic -std=gnu99 -Og -ggdb -Wall")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

set(SOURCE
    src/main.c
    src/config.c
    src/exec.c
    src/modules.c
    src/render.c
    src/socket.c
    src/tray.c
    src/util.c
    src/window.c
)

if (NOT DEFINED LIBDIR)
    set(LIBDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif()

add_definitions(-D_WITH_DPRINTF)
add_definitions(-DLIBDIR="${LIBDIR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(pangocairo REQUIRED pangocairo)
link_directories(${pangocairo_LIBRARY_DIRS})

add_executable(blockbar ${SOURCE})
add_executable(bbc src/bbc.c)

find_library(cairo cairo)
find_library(dl dl)
find_library(ujson ujson)
find_library(X11 X11)
find_library(xrandr Xrandr)
target_link_libraries(blockbar ${cairo} ${dl} ${pangocairo_LIBRARIES} ${X11} ${xrandr} ${ujson})

target_include_directories(blockbar PUBLIC ${pangocairo_INCLUDE_DIRS})
target_compile_options(blockbar PUBLIC ${pangocairo_CFLAGS_OTHER})

add_custom_command(
    OUTPUT doc/blockbar.1.gz
    DEPENDS doc/blockbar.1
    COMMAND gzip -f < doc/blockbar.1 > doc/blockbar.1.gz)

add_custom_target(
    man ALL DEPENDS doc/blockbar.1.gz)

install(TARGETS blockbar bbc
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES "doc/blockbar.1.gz"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
    COMPONENT doc)

install(DIRECTORY include/blockbar
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")

macro(module name)
add_custom_command(
    OUTPUT modules/${name}/${name}.so
    COMMAND make -C modules/${name})

add_custom_target(${name} ALL DEPENDS modules/${name}/${name}.so)

install(FILES "modules/${name}/${name}.so"
    DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/blockbar"
    COMPONENT module)
endmacro(module)

module(legacy)
module(subblocks)
