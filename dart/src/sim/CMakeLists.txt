cmake_minimum_required(VERSION 3.5)
project(sim)

set(CMAKE_CXX_STANDARD 11)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if (LINUX)

find_package(PkgConfig REQUIRED)

pkg_search_module(UUID REQUIRED uuid)

endif()



set(SOURCE_FILES
        dart_random_number.c
        dart_random_number.h
        dart_commons.c
        dart_commons.h
        dart_sim.c
        dart_sim.h
        dart_utils.c
        dart_utils.h)

add_executable(sim.exe ${SOURCE_FILES})
target_include_directories(sim.exe PUBLIC ${UUID_INCLUDE_DIRS} ${MATH_INCLUDE_DIRS})
target_link_libraries(sim.exe ${UUID_LIBRARIES} ${MATH_INCLUDE_DIRS})
