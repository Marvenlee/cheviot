set(CFLAGS "--march=armv6")
set(CMAKE_C_FLAGS "--march=armv6")

ExternalProject_Add (
	kernel
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/kernel	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/kernel/configure --host=arm-none-eabi
	                   CFLAGS=-march=armv6 --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/host/boot
	BUILD_ALWAYS      OFF
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/host/boot
	DEPENDS           newlib
	BUILD_COMMAND     make
)


