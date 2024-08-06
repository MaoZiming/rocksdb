#
# Dependencies
#
find_package(Threads)

# Specify the path to the RocksDB installation
set(ROCKSDB_INCLUDE_DIR "/usr/local/include")
set(ROCKSDB_LIB_DIR "/usr/local/lib")

include_directories(${ROCKSDB_INCLUDE_DIR})

# Find the RocksDB library
find_library(ROCKSDB_LIBRARY rocksdb HINTS ${ROCKSDB_LIB_DIR})

#
# Sources
#
set(SOURCES
    src/main.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})


#
# Target
#
add_executable(server ${SOURCES})
target_link_libraries(server
    PRIVATE
        myproto
        ${ROCKSDB_LIBRARY}
)