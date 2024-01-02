cmake_minimum_required(VERSION 3.5)

include(ExternalProject)


ExternalProject_Add (
	libtask
  URL https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/libtask/libtask-20121021.tgz
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtask/
	PATCH_COMMAND     patch -p2 --forward --input=${CMAKE_CURRENT_SOURCE_DIR}/patches/libtask.patch
	                  && chmod +x ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtask/configure	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libtask/configure --host=arm-none-eabi
	                    --disable-newlib-supplied-syscalls
	                    --enable-interwork --enable-multilib
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/native/
	                    --includedir=${CMAKE_CURRENT_BINARY_DIR}/build/native/arm-none-eabi/include/
	                    --libdir=${CMAKE_CURRENT_BINARY_DIR}/build/native/arm-none-eabi/lib/
	BUILD_ALWAYS      OFF
  DEPENDS           gcc-native binutils-native newlib
  INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/native/
	BUILD_COMMAND     make
  INSTALL_COMMAND   make install
)


