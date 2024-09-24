#
# Dependencies
#
find_package(Threads REQUIRED)

# Specify the path to the RocksDB installation
set(ROCKSDB_INCLUDE_DIR "/usr/local/include")
set(ROCKSDB_LIB_DIR "/usr/local/lib")

include_directories(${ROCKSDB_INCLUDE_DIR})

# Find the RocksDB library
find_library(ROCKSDB_LIBRARY rocksdb HINTS ${ROCKSDB_LIB_DIR})

# Check if RocksDB was found
if(NOT ROCKSDB_LIBRARY)
    message(FATAL_ERROR "RocksDB library not found!")
endif()

#
# Sources
#
set(SOURCES
    src/load_tracker.cpp
    src/thread_pool.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})

#
# Target
#
add_executable(server
    ${SOURCES}
    src/main.cpp
)

# Link the necessary libraries, including threading support
target_link_libraries(server
    PRIVATE
    myproto
    ${ROCKSDB_LIBRARY}
    Threads::Threads
)
