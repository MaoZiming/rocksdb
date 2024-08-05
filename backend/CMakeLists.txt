cmake_minimum_required(VERSION 3.5)

project(RocksDBGrpc)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(protobuf CONFIG REQUIRED) # CONFIG is important. 
find_package(gRPC CONFIG REQUIRED)
find_package(Threads)

# Specify the path to the RocksDB installation
set(ROCKSDB_INCLUDE_DIR "/usr/local/include")
set(ROCKSDB_LIB_DIR "/usr/local/lib")

include_directories(${Protobuf_INCLUDE_DIRS} ${gRPC_INCLUDE_DIRS} ${ROCKSDB_INCLUDE_DIR})

# Find the RocksDB library
find_library(ROCKSDB_LIBRARY rocksdb HINTS ${ROCKSDB_LIB_DIR})

add_executable(rocksdb_server rocksdb_server.cpp rocksdb_service.pb.cc rocksdb_service.grpc.pb.cc)

# Link the RocksDB library along with Protobuf and gRPC libraries
target_link_libraries(rocksdb_server ${Protobuf_LIBRARIES} ${gRPC_LIBRARIES} ${ROCKSDB_LIBRARY})
