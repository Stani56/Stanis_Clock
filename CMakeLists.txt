cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "components")

# Use custom partition table for larger app size
set(PARTITION_TABLE_CUSTOM_FILENAME "partitions.csv")
set(CONFIG_PARTITION_TABLE_CUSTOM ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wordclock)
