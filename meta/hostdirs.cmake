# Setup the build/host directory structure, effectively laying the keel
# of the project. See hostdirs/Makefile.am for the directory creation.

ExternalProject_Add (
	hostdirs
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/hostdirs	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/hostdirs/configure
	                   --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/host/
	BUILD_ALWAYS      OFF
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/host/
	DEPENDS           
	INSTALL_COMMAND   pseudo make install
)

