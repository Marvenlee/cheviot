cmake_minimum_required(VERSION 3.5)

message(status "CXX is " $ENV{BOARD})
if(DEFINED ENV{BOARD})
    message(status "BOARD is defined and it is " $ENV{BOARD})
    if($ENV{BOARD} MATCHES "raspberrypi1")
        message(status "installing raspberry pi 1 boot partition blobs")

        file(COPY
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/blobs/rpi1/bootcode.bin
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/blobs/rpi1/start.elf
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/build/boot_partition)

    elseif($ENV{BOARD} MATCHES "raspberrypi4")
        message(status "installing raspberry pi 4 boot partition blobs")

        file(COPY
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/blobs/rpi4/start4.elf
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/blobs/rpi4/fixup4.dat
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/blobs/rpi4/config.txt
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/build/boot_partition)

    else()
      message(status "BOARD is unknown")
    endif()
    
else()
    message(status "BOARD is undefined")
endif()





