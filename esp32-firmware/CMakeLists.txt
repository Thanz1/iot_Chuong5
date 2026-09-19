cmake_minimum_required(VERSION 3.16)

# Tránh crash GCC 14 do esp_lcd
set(EXCLUDE_COMPONENTS esp_lcd)

list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/common_components/protocol_examples_common")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(iot-esp32-firmware)