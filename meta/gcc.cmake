cmake_minimum_required(VERSION 3.5)

include(ExternalProject)


ExternalProject_Add (
	gcc-native
	URL               https://ftp.gnu.org/gnu/gcc/gcc-10.5.0/gcc-10.5.0.tar.xz
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gcc-10.5.0
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gcc-10.5.0/configure --target=arm-none-eabi
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/native 
	                    --without-headers
	                    --with-newlib --with-gnu-as --with-gnu-ld
	                    --enable-languages=c
	BUILD_ALWAYS      OFF
	TEST_AFTER_INSTALL 1
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/native/
  DEPENDS           binutils-native
	BUILD_COMMAND     make all-gcc
	INSTALL_COMMAND   make install-gcc
	TEST_COMMAND      make all install
	STEP_TARGETS      install test

)

