cmake_minimum_required(VERSION 3.10)

project(blockbar)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Og -ggdb -Wall")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

set(SOURCE
    src/main.c
    src/config.c
)

add_executable(blockbar ${SOURCE})

find_library(ujson ujson)
target_link_libraries(blockbar ${ujson})
