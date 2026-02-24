cmake_minimum_required(VERSION 3.22)

project(VSTLibrary VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(EXISTS "${CMAKE_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    message(FATAL_ERROR "JUCE not found. Clone JUCE into ${CMAKE_SOURCE_DIR}/JUCE")
endif()

add_subdirectory(Plugins)
