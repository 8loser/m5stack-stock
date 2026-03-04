cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    components/board
    components/storage
    components/device_server
    components/telegram_bot
    components/twse_client
    components/scheduler
    components/ui
)
set(EXCLUDE_COMPONENTS ai_provider)

# Shared project headers (e.g. app_config.h) used by multiple components.
include_directories(${CMAKE_SOURCE_DIR}/include)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(m5stack_stock)
