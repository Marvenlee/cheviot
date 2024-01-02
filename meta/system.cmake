cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

ExternalProject_Add (
	system
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/system	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/system/configure --host=arm-none-eabi
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/host	
	BUILD_ALWAYS      OFF
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/host
	DEPENDS           newlib libtask cheviot-libs
	BUILD_COMMAND     make
)



