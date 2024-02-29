cmake_minimum_required(VERSION 3.5)

project(Cheviot)

include(ExternalProject)

set(CMAKE_CXX_STANDARD 11) 
set(ENV{PSEUDO_PREFIX} ${CMAKE_CURRENT_BINARY_DIR}/build/native/)

# Build the pseudo tool. This is not part of the 'all' target and
# must be done separately with 'make pseudo-native'
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/pseudo.cmake)

# Lay out the root filesytem's directory structure in the build/host directory
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/hostdirs.cmake)

add_custom_target(host_apps)

# Build and install GCC, binutils, newlib and build-tools on build platform

include(${CMAKE_CURRENT_SOURCE_DIR}/meta/mkifs.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/binutils.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/gcc.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/newlib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/gcc-post-newlib.cmake)

# Build and install other libraries on build platform

include(${CMAKE_CURRENT_SOURCE_DIR}/meta/hal.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/libtask.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/cheviot-libs.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/bsdlibs.cmake)

# Build kernel and system apps for host platform

include(${CMAKE_CURRENT_SOURCE_DIR}/meta/kernel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/system.cmake)

# Build third party tools for host platform

include(${CMAKE_CURRENT_SOURCE_DIR}/meta/bsdutils.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/pdksh.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/coreutils.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/less.cmake)

# Copy third_party blobs to the build/host/boot_partition
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/boot-fs.cmake)

# Build the partition images and combined sdcard image
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/image.cmake)

