cmake_minimum_required(VERSION 3.5)

include(ExternalProject)


ExternalProject_Add (
	hal
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/hal
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/hal/configure --host=arm-none-eabi
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/native/
	                    --includedir=${CMAKE_CURRENT_BINARY_DIR}/build/native/arm-none-eabi/include/
	                    --libdir=${CMAKE_CURRENT_BINARY_DIR}/build/native/arm-none-eabi/lib/
	BUILD_ALWAYS      YES
  DEPENDS           gcc-native binutils-native newlib hostdirs
  INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/native/
	BUILD_COMMAND     make
  INSTALL_COMMAND   make install
)



