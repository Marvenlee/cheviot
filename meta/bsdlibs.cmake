cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

ExternalProject_Add (
	bsdlibs
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bsdlibs	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bsdlibs/configure 
	                    --host=arm-none-eabi
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/native/
	                    --includedir=${CMAKE_CURRENT_BINARY_DIR}/build/native/arm-none-eabi/include/
	                    --libdir=${CMAKE_CURRENT_BINARY_DIR}/build/native/arm-none-eabi/lib/
	BUILD_ALWAYS      OFF
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/native/
	DEPENDS           newlib hal cheviot-libs 
	BUILD_COMMAND     make
	INSTALL_COMMAND   pseudo make install
)

