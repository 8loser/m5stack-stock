cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    components/board
    components/storage
    components/wifi_manager
    components/twse_client
    components/ai_provider
    components/scheduler
    components/ui
)

# Shared project headers (e.g. app_config.h) used by multiple components.
include_directories(${CMAKE_SOURCE_DIR}/include)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(m5stack_stock)
