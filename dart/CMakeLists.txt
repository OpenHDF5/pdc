cmake_minimum_required (VERSION 2.8.11)


project(DART C)


if(DEFINED ENV{ENABLE_MPI})
  if("$ENV{ENABLE_MPI}" STREQUAL "")
    set(ENABLE_MPI OFF CACHE BOOL "")
  else()
    set(ENABLE_MPI $ENV{ENABLE_MPI})
  endif()
  if(ENABLE_MPI)
    set(ENABLE_MPI ON CACHE BOOL "")
  else()
    set(ENABLE_MPI OFF CACHE BOOL "")
  endif()
else()
  set(ENABLE_MPI OFF CACHE BOOL "")
endif()



#------------------------------------------------------------------------------
# Version information
#------------------------------------------------------------------------------
set(DART_VERSION_MAJOR "0")
set(DART_VERSION_MINOR "1")
set(DART_VERSION_PATCH "0")
set(DART_PACKAGE "dart")
set(DART_PACKAGE_NAME "DART")
set(DART_PACKAGE_VERSION "${DART_VERSION_MAJOR}.${DART_VERSION_MINOR}.${DART_VERSION_PATCH}")
set(DART_PACKAGE_VERSION_MAJOR "${DART_VERSION_MAJOR}.${DART_VERSION_MINOR}")
set(DART_PACKAGE_VERSION_MINOR "${DART_VERSION_PATCH}")
set(DART_PACKAGE_STRING "${DART_PACKAGE_NAME} ${DART_PACKAGE_VERSION}")
set(DART_PACKAGE_TARNAME "${DART_PACKAGE}")

#------------------------------------------------------------------------------
# Setup install and output Directories
#------------------------------------------------------------------------------
if(NOT DART_INSTALL_BIN_DIR)
  set(DART_INSTALL_BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
endif()
if(NOT DART_INSTALL_LIB_DIR)
  set(DART_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
endif()
if(NOT DART_INSTALL_INCLUDE_DIR)
  set(DART_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
endif()
if(NOT DART_INSTALL_DATA_DIR)
  set(DART_INSTALL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share)
endif()

# Setting this ensures that "make install" will leave rpaths to external
# libraries intact on "make install". This ensures that one can install a
# version of DART on the build machine without any issues. If this not
# desired, simply specify CMAKE_INSTALL_RPATH_USE_LINK_PATH when configuring
# DART and "make install" will strip all rpaths, which is default behavior.
if(NOT CMAKE_INSTALL_RPATH_USE_LINK_PATH)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

#------------------------------------------------------------------------------
# Setup CMake Environment
#------------------------------------------------------------------------------
if(APPLE)
  # We are doing a unix-style install i.e. everything will be installed in
  # CMAKE_INSTALL_PREFIX/bin and CMAKE_INSTALL_PREFIX/lib etc. as on other unix
  # platforms. We still need to setup CMAKE_INSTALL_NAME_DIR correctly so that
  # the binaries point to appropriate location for the libraries.

  # 1. Make CMAKE_INSTALL_PREFIX publicly accessible, if it was hidden in
  #    previous pass
  get_property(is_internal CACHE CMAKE_INSTALL_PREFIX PROPERTY TYPE)
  if(is_internal STREQUAL "INTERNAL")
    set(CMAKE_INSTALL_PREFIX ${CACHED_CMAKE_INSTALL_PREFIX} CACHE PATH "Install prefix" FORCE)
  else()
    set(CMAKE_INSTALL_PREFIX ${CACHED_CMAKE_INSTALL_PREFIX} CACHE PATH "Install prefix")
  endif()
  unset(MACOSX_APP_INSTALL_PREFIX CACHE)

  set(CMAKE_INSTALL_NAME_DIR "@rpath")
  mark_as_advanced(
    CMAKE_OSX_ARCHITECTURES
    CMAKE_OSX_DEPLOYMENT_TARGET
    CMAKE_OSX_SYSROOT
    )
endif()

#------------------------------------------------------------------------------
if(NOT DART_EXTERNALLY_CONFIGURED)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
      ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables."
  )
  set(EXECUTABLE_OUTPUT_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
      ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Libraries"
  )
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
      ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all static libraries."
  )
endif()
set(DART_CMAKE_DIR "${DART_SOURCE_DIR}/CMake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${DART_CMAKE_DIR})

#------------------------------------------------------------------------------
# Disallow in-source build
#------------------------------------------------------------------------------
if("${DART_SOURCE_DIR}" STREQUAL "${DART_BINARY_DIR}")
  message(FATAL_ERROR
    "DART requires an out of source Build. "
    "Please create a separate binary directory and run CMake there.")
endif()

#------------------------------------------------------------------------------
# Set a default build type if none was specified
#------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

if(NOT CMAKE_C_FLAGS AND CMAKE_COMPILER_IS_GNUCC)
  message(STATUS "GCC detected, setting additional flags")
  set(CMAKE_C_FLAGS "-Wall -Wextra -Winline -Wcast-qual -std=gnu99 -Wshadow" CACHE STRING "Flags used by the compiler during all build types." FORCE)
endif()

#-----------------------------------------------------------------------------
# Targets built within this project are exported at Install time for use
# by other projects.
#-----------------------------------------------------------------------------
if(NOT DART_EXPORTED_TARGETS)
  set(DART_EXPORTED_TARGETS "dart-targets")
endif()

#------------------------------------------------------------------------------
# Choose static or shared libraries.
#------------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Build with shared libraries." OFF)

#-------------------------------------------------------------------------------
function(DART_set_lib_options libtarget libname libtype)
  if(${libtype} MATCHES "SHARED")
    if(WIN32 AND NOT MINGW)
      set(LIB_RELEASE_NAME "${libname}")
      set(LIB_DEBUG_NAME "${libname}_D")
      set(LIB_VERSION ${DART_PACKAGE_VERSION_MAJOR})
    else()
      set(LIB_RELEASE_NAME "${libname}")
      set(LIB_DEBUG_NAME "${libname}_debug")
      set(LIB_VERSION ${DART_PACKAGE_VERSION})
    endif()
  else()
    if(WIN32 AND NOT MINGW)
      set(LIB_RELEASE_NAME "lib${libname}")
      set(LIB_DEBUG_NAME "lib${libname}_D")
    else()
      # if the generator supports configuration types or if the CMAKE_BUILD_TYPE has a value
      if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE)
        set(LIB_RELEASE_NAME "${libname}")
        set(LIB_DEBUG_NAME "${libname}_debug")
      else()
        set(LIB_RELEASE_NAME "lib${libname}")
        set(LIB_DEBUG_NAME "lib${libname}_debug")
      endif()
    endif()
  endif()

  set_target_properties(${libtarget}
      PROPERTIES
      DEBUG_OUTPUT_NAME          ${LIB_DEBUG_NAME}
      RELEASE_OUTPUT_NAME        ${LIB_RELEASE_NAME}
      MINSIZEREL_OUTPUT_NAME     ${LIB_RELEASE_NAME}
      RELWITHDEBINFO_OUTPUT_NAME ${LIB_RELEASE_NAME}
      VERSION                    ${LIB_VERSION}
      SOVERSION                  ${LIB_VERSION}
  )

  #----- Use MSVC Naming conventions for Shared Libraries
  if(MINGW AND ${libtype} MATCHES "SHARED")
    set_target_properties(${libtarget}
        PROPERTIES
        IMPORT_SUFFIX ".lib"
        IMPORT_PREFIX ""
        PREFIX ""
    )
  endif()
endfunction()

#-----------------------------------------------------------------------------
# Source
#-----------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/utils)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/commons)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/client)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/server)

#-----------------------------------------------------------------------------
# Testing
#-----------------------------------------------------------------------------
option(BUILD_TESTING "Build testing." OFF)
if(NOT DART_EXTERNALLY_CONFIGURED AND BUILD_TESTING)
  enable_testing()
  include(CTest)
  add_subdirectory(tests)
endif()

#-----------------------------------------------------------------------------
# Build doxygen documentation.
#-----------------------------------------------------------------------------
option(BUILD_DOCUMENTATION "Build documentation." OFF)
if(BUILD_DOCUMENTATION)
  add_subdirectory(doc/doxygen)
endif()

#-----------------------------------------------------------------------------
# Configure the config.cmake file for the build directory
#-----------------------------------------------------------------------------
configure_file(
  ${DART_SOURCE_DIR}/CMake/dart-config.cmake.build.in
  ${DART_BINARY_DIR}/dart-config.cmake @ONLY
)
configure_file(
  ${DART_SOURCE_DIR}/CMake/DART_utils-config.cmake.build.in
  ${DART_BINARY_DIR}/DART_utils-config.cmake @ONLY
)

#-----------------------------------------------------------------------------
# Configure the config.cmake file for the install directory
#-----------------------------------------------------------------------------
configure_file(
  ${DART_SOURCE_DIR}/CMake/dart-config.cmake.install.in
  ${DART_BINARY_DIR}/CMakeFiles/dart-config.cmake @ONLY
)
configure_file(
  ${DART_SOURCE_DIR}/CMake/DART_utils-config.cmake.install.in
  ${DART_BINARY_DIR}/CMakeFiles/DART_utils-config.cmake @ONLY
)

install(
  FILES
    ${DART_BINARY_DIR}/CMakeFiles/dart-config.cmake
    ${DART_BINARY_DIR}/CMakeFiles/DART_utils-config.cmake
  DESTINATION
    ${DART_INSTALL_DATA_DIR}/cmake/dart
)

#-----------------------------------------------------------------------------
# Configure the dart-config-version .cmake file for the install directory
#-----------------------------------------------------------------------------
configure_file(
  ${DART_SOURCE_DIR}/CMake/dart-config-version.cmake.in
  ${DART_BINARY_DIR}/CMakeFiles/dart-config-version.cmake @ONLY
)
configure_file(
  ${DART_SOURCE_DIR}/CMake/DART_utils-config-version.cmake.in
  ${DART_BINARY_DIR}/CMakeFiles/DART_utils-config-version.cmake @ONLY
)

install(
  FILES
    ${DART_BINARY_DIR}/CMakeFiles/dart-config-version.cmake
    ${DART_BINARY_DIR}/CMakeFiles/DART_utils-config-version.cmake
  DESTINATION
    ${DART_INSTALL_DATA_DIR}/cmake/dart
)

#-----------------------------------------------------------------------------
# CPack
#-----------------------------------------------------------------------------
if(NOT DART_EXTERNALLY_CONFIGURED)
  set(CPACK_PACKAGE_DESCRIPTION_FILE ${DART_SOURCE_DIR}/README.md)
#  set(CPACK_RESOURCE_FILE_LICENSE ${DART_SOURCE_DIR}/COPYING)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Proactive Data Containers")
  set(CPACK_PACKAGE_NAME "DART")
  set(CPACK_PACKAGE_VENDOR "LBNL / The HDF Group / ANL")
  set(CPACK_PACKAGE_VERSION_MAJOR ${DART_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${DART_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${DART_VERSION_PATCH})
  set(CPACK_GENERATOR "TBZ2")
  set(CPACK_SOURCE_PACKAGE_FILE_NAME ${DART_PACKAGE_TARNAME}-${DART_PACKAGE_VERSION})
  set(CPACK_SOURCE_IGNORE_FILES ".git*;/GitSetup/;/.git/;.swp$;.#;/#;.*~")
  set(CPACK_SOURCE_STRIP_FILES "")
  include(CPack)
endif()
