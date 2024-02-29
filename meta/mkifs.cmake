# Build the mkifs tool in order to generate the kernel.img which
# comprises of the bootloader and read-only Initial File System (IFS)
# concatenated together.
#
# The IFS contains a simple filesystem containing the kernel and
# the required drivers in order to bootstrap the system.  These include:
#
# kernel
# ifs filesystem handler (acts as root and IFS handler)
# aux uart driver
# init and startup.cfg (runs simple init script)
# dev filesystem for /dev mount
# sdcard driver
# ext2 filesystem handler
#
# Optionally this can contain the ksh shell and other command line utilities

cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

ExternalProject_Add (
	mkifs
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/tools/mkifs	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/mkifs/configure
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/native
	BUILD_ALWAYS      OFF
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/native
	BUILD_COMMAND     make
)

