cmake_minimum_required(VERSION 3.22)
project(rf24-lib C)

# Compiler flags cho STM32F103C8T6 (Cortex-M3)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-mcpu=cortex-m3 -mthumb -O2 -Wall")

# Include path
include_directories(${PROJECT_SOURCE_DIR})

# Source files
set(SOURCES
    rf24_lib.c
)

# Tạo static library
add_library(rf24-lib STATIC ${SOURCES})
