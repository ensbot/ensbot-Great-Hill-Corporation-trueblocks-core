# minimum cmake version supported
cmake_minimum_required (VERSION 3.0)

# folder project
project (go-apps)

# Path to library include files
include_directories(${BASE_INCLUDE})

# messaging
# message (MESSAGE "Entering go-apps folder")

# Compilation order of the src subdirectories
add_subdirectory(blaze)
