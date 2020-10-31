cmake_minimum_required(VERSION 3.5)

include(ExternalProject)

ExternalProject_Add_StepDependencies(gcc-native test newlib-interim)

add_custom_target(newlib 
    ALL
    DEPENDS gcc-native-test
)




