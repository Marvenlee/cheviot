cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

# Build and install GCC, binutils, newlib and build-tools on build platform

include(${CMAKE_CURRENT_SOURCE_DIR}/meta/mkifs.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/binutils.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/gcc.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/newlib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/gcc-post-newlib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/libtask.cmake)

# Build kernel and system apps for host platform

include(${CMAKE_CURRENT_SOURCE_DIR}/meta/kernel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/system.cmake)

# Build third party tools for host platform

include(${CMAKE_CURRENT_SOURCE_DIR}/meta/pdksh.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/coreutils.cmake)


# Build Kernel.img containing kernel and embedded IFS (Initial File System)
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/boot-fs.cmake)

# Build flash disk image
include(${CMAKE_CURRENT_SOURCE_DIR}/meta/image.cmake)

