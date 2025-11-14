cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "components")

# Use custom partition table for larger app size
set(PARTITION_TABLE_CUSTOM_FILENAME "partitions.csv")
set(CONFIG_PARTITION_TABLE_CUSTOM ON)

# Read version from version.txt (fixed version, not git-based)
file(STRINGS "${CMAKE_SOURCE_DIR}/version.txt" FIRMWARE_VERSION LIMIT_COUNT 1)
string(STRIP "${FIRMWARE_VERSION}" FIRMWARE_VERSION)
set(PROJECT_VER ${FIRMWARE_VERSION})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wordclock)
