cmake_minimum_required(VERSION 3.16)
project(ex4)

set(CMAKE_CXX_STANDARD 11)

add_executable(ex4 main.cpp)

add_executable(physical_memory PhysicalMemory.cpp PhysicalMemory.h)

add_executable(ex_4_our SimpleTest.cpp VirtualMemory.h VirtualMemory.cpp PhysicalMemory.h PhysicalMemory.cpp)

add_executable(test_1 YaaraConstants.h YaaraTest.cpp VirtualMemory.h VirtualMemory.cpp PhysicalMemory.h PhysicalMemory.cpp )