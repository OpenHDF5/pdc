cmake_minimum_required(VERSION 3.8)
project(sim)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        dart_commons.c
        dart_commons.h
        dart_sim.c
        dart_sim.h
        dart_utils.c
        dart_utils.h)

add_executable(sim.exe ${SOURCE_FILES})