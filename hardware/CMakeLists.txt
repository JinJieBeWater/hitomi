cmake_minimum_required(VERSION 3.16.0)

if(NOT DEFINED ENV{HITOMI_ARDUINO_COMPONENT_DIR})
    message(FATAL_ERROR "Missing HITOMI_ARDUINO_COMPONENT_DIR for Arduino component injection")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "$ENV{HITOMI_ARDUINO_COMPONENT_DIR}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hitomi_hardware_runtime)
