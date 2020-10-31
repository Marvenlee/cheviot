cmake_minimum_required(VERSION 3.5)

include(ExternalProject)


ExternalProject_Add (
	coreutils
    URL               https://ftp.gnu.org/gnu/coreutils/coreutils-5.2.1.tar.bz2
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/coreutils-5.2.1
	PATCH_COMMAND     patch -s -p2 --forward --input=${CMAKE_CURRENT_SOURCE_DIR}/patches/coreutils-5.2.1.patch	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/third_party/coreutils-5.2.1/configure --host=arm-none-eabi
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/host
                        CFLAGS=-D_POSIX_VERSION host_alias=arm-none-eabi
	DEPENDS           newlib
	BUILD_ALWAYS      OFF
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/host/
	BUILD_COMMAND     make
)

