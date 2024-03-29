cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

ExternalProject_Add (
  newlib-interim
  URL               ftp://sourceware.org/pub/newlib/newlib-4.1.0.tar.gz
  PATCH_COMMAND     patch -s -p2 --forward --input=${CMAKE_CURRENT_SOURCE_DIR}/patches/newlib-4.1.0.patch	
  PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
  SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/newlib-4.1.0/	
  CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/third_party/newlib-4.1.0/configure
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/native/
	                    --target=arm-none-eabi --disable-newlib-supplied-syscalls
	                    --enable-interwork --enable-multilib	                    
  BUILD_ALWAYS      OFF
  DEPENDS           gcc-native-install binutils-native hostdirs
  INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/native/
  BUILD_COMMAND     make
  INSTALL_COMMAND   make install
)

