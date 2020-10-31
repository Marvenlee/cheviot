cmake_minimum_required(VERSION 3.5)

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/blobs/bootcode.bin
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/blobs/start.elf
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/build/boot_partition)



