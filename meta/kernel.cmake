
ExternalProject_Add (
	kernel
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/kernel	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/kernel/configure --host=arm-none-eabi
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/host/boot
	BUILD_ALWAYS      OFF
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/host/boot
	DEPENDS           newlib
	BUILD_COMMAND     make
)


