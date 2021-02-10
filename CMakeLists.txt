# Note: gmic requires CMake 3.14
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(vips-gmic
        VERSION 0.0.1
        DESCRIPTION "libvips plugin for running gmic commands"
        LANGUAGES
            CXX
        )

# Let's enable C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG") # Optimize for performance
endif()

include(FetchContent)

# Use GNUInstallDirs to install libraries into correct locations on all platforms
include(GNUInstallDirs)

# Find vips (required)
find_package(PkgConfig)
pkg_check_modules(VIPS vips>=8.8 REQUIRED)

# Add gmic dependency
add_subdirectory(third_party/gmic)

add_subdirectory(src)
