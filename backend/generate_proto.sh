protoc -I . --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` rocksdb_service.proto
protoc -I . --cpp_out=. rocksdb_service.proto
