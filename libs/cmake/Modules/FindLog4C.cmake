##############################################################################
# Author:     Ross G. Miller
#             Oak Ridge National Lab
# Date:       04/02/20
# Purpose:    A CMake file for locating the Log4C library and header
#
# Copyright 2020 UT Battelle, LLC
#
# This work was supported by the Oak Ridge Leadership Computing Facility at
# the Oak Ridge National Laboratory, which is managed by UT Battelle, LLC for
# the U.S. DOE (under the contract No. DE-AC05-00OR22725).
#
# This file is part of the Spectral project.
##############################################################################

# NOTE:  This is my first attempt at writing a "find" module for CMake, and
# I'm not certain I've done it correctly.  Proceed accordingly...

# 
# Try to find Log4C files
# Once done, this will define the following variables:
#  Log4C_FOUND        - system has Log4C
#  Log4C_INCLUDE_DIRS - the Log4C include directories
#
# and also the following imported target:
#  Log4C::Log4C

# Log4C apparently doesn't use pkg-config, so we have to do
# the searching manually.  It's generally installed into the
# standard library and include directories, though.

# Include dir
find_path(Log4C_INCLUDE_DIR
  NAMES log4c.h
)

# The library itself
find_library(Log4C_LIBRARY
  NAMES log4c
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Log4C
  FOUND_VAR Log4C_FOUND
  REQUIRED_VARS
    Log4C_LIBRARY
    Log4C_INCLUDE_DIR
  VERSION_VAR Foo_VERSION
)

# Set the Log4C* variables to only show up when the user is in 'advanced mode'
# in the GUI or TUI.  (For reasons that are unclear, Log4C_FOUND never shows
# up, regardless of the mode.  Maybe it's not created as a cached variable?)
mark_as_advanced(Log4C_FOUND, Log4C_INCLUDE_DIR Log4C_LIBRARY)

if(Log4C_FOUND)
  message( STATUS "Log4C_INCLUDE_DIR = ${Log4C_INCLUDE_DIR}" )
  message( STATUS "Log4C_LIBRARY = ${Log4C_LIBRARY}" )
  # Note: Starting in CMake 3.15, we've got more detailed message types such
  #       as DEBUG and TRACE.

  # Regex string for extracting parts of the library path name.
  # First capture group returns the directory path, second capture group
  # returns the part between "lib" and ".so"
  # Note 1: I find it hard to believe that this is the proper way to tell
  #         CMake the library's name and location.
  # Note 2: "(.:\\.*)\\(.*)\.dll" will work if we ever need Windows support.
  # Note 3: This is only used for the 'IMPORTED INTERFACE' and old-school
  #         methods.  If we end up using the 'SHARED IMPORTED' technique, we
  #         probably won't need any of it.)
  set (LIB_REGEX_STRING "(.*)\/lib(.*)\.so")
  string(REGEX REPLACE "${LIB_REGEX_STRING}" "\\2" Log4C_LIB_NAME "${Log4C_LIBRARY}")
  string(REGEX REPLACE "${LIB_REGEX_STRING}" "\\1" Log4C_LIB_DIR "${Log4C_LIBRARY}")
  message( STATUS "Log4C_LIB_DIR = ${Log4C_LIB_DIR} (regex match)" )
  message( STATUS "Log4C_LIB_NAME = ${Log4C_LIB_NAME} (regex match)" )

else()
  message( WARNING "Log4C was not found!")
endif()

set(USE_IMPORTED_INTERFACE 0)
set(USE_SHARED_IMPORTED 1)

if (USE_IMPORTED_INTERFACE)
  # Create the target as an imported interface
  # According to the docs, this isn't exactly correct since INTERFACE targets
  # aren't supposed to be backed by anything on disc (such as a share library
  # file).  The cannonical use-case is for header-only packages.
  # That said, this code at least generates a '-llog4c' on the linker command
  # line.  What I can't figure out how to do is make it generate the appropriate
  # "-L<dir>" option, so the build fails unless the library just happens to be
  # located in a directory that is searched by default.
  if(Log4C_FOUND AND NOT TARGET Log4C::Log4C)
    add_library(Log4C::Log4C IMPORTED INTERFACE)
    set_target_properties(Log4C::Log4C PROPERTIES
  #    IMPORTED_LOCATION "${Log4C_Library}"
      INTERFACE_LINK_LIBRARIES "${Log4C_Library}"
      IMPORTED_LIBNAME "${Log4C_LIB_NAME}"
      INTERFACE_LINK_DIRECTORIES "/tmp"
      INTERFACE_INCLUDE_DIRECTORIES "${Log4C_INCLUDE_DIR}" )
  endif()
elseif(USE_SHARED_IMPORTED)
  # Create the target as an shared imported library
  # According to the docs, this is the method I should use.  Unfortunately,
  # it generates an odd linker command line.  Instead of appropriate "-L" and
  # "-l" options, it adds the full path to the shared libary as if it were
  # an object file.  This seems to work, but it just doesn't look right.
  if(Log4C_FOUND AND NOT TARGET Log4C::Log4C)
    add_library(Log4C::Log4C SHARED IMPORTED)
    set_target_properties(Log4C::Log4C PROPERTIES
      IMPORTED_LOCATION "${Log4C_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Log4C_INCLUDE_DIR}"
    )
  endif()
else()
  # This is the old-school (and now frowned upon) method: just set some 
  # variables and let the upstream CMakeLists.txt file use them.

  # The convention seems to be to use DIRS (plural) rather than DIR
  set( Log4C_INCLUDE_DIRS ${Log4C_INCLUDE_DIR})
    set( Log4C_LINK_FLAGS "-L${Log4C_LIB_DIR}") # We're hard-coding the '-L'.  That's not good!
  # TODO: if it's /usr/lib, we should set the link flags to the empty string.
  message( STATUS "Log4C_INCLUDE_DIRS = ${Log4C_INCLUDE_DIR}")
  message( STATUS "Log4C_LINK_FLAGS = -L${Log4C_LIB_DIR}")
endif()