cmake_minimum_required(VERSION 3.1)

set(STORAGE_TARGET s3storageplugin)

project(${STORAGE_TARGET})

include_directories("../../../include")

add_definitions(
    -DNOSSL
    -DNOLFS
)

if(WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
include_directories(.)
add_library(${STORAGE_TARGET} SHARED
#        "impl/s3lib.h"
#        "impl/s3lib.cpp"
        "s3_library.h"
        "s3_library.cpp"
)

if(WINDOWS)
    target_link_libraries(${STORAGE_TARGET} ws2_32)
endif()
target_link_libraries(${STORAGE_TARGET} s3)
set_target_properties(${STORAGE_TARGET} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
#set(CMAKE_EXE_LINKER_FLAGS "-static")
