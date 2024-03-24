cmake_minimum_required(VERSION 3.26)
project(memory_allocation_gui C)

set(CMAKE_C_STANDARD 11)

add_executable(memory_allocation_gui
        heap.c
        heap.h
        custom_unistd.h)
