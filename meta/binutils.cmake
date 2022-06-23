cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

ExternalProject_Add (
	binutils-native
#	URL               https://ftp.gnu.org/gnu/binutils/binutils-2.25.tar.bz2
	PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/output
	SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/binutils-2.25	
	CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/third_party/binutils-2.25/configure --target=arm-none-eabi
	                    --prefix=${CMAKE_CURRENT_BINARY_DIR}/build/native
                        --disable-nls --disable-multilib --with-gnu-as --with-gnu-ld --disable-libssp
                        --enable-interwork --enable-multilib          
	BUILD_ALWAYS      OFF
	INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}/build/native/
	BUILD_COMMAND     make
)

