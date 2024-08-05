#include <grpcpp/grpcpp.h>
#include <rocksdb/db.h>

#include <iostream>
#include <memory>
#include <string>

#include "rocksdb_service.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using rocksdb::PutRequest;
using rocksdb::PutResponse;
using rocksdb::RocksDBService;

class RocksDBServiceImpl final : public RocksDBService::Service {
 public:
  RocksDBServiceImpl(const std::string& db_path) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_);
    if (!status.ok()) {
      std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
      exit(1);
    }
  }

  ~RocksDBServiceImpl() { delete db_; }

  Status Put(ServerContext* context, const PutRequest* request,
             PutResponse* response) override {
    rocksdb::Status s =
        db_->Put(rocksdb::WriteOptions(), request->key(), request->value());
    response->set_success(s.ok());
    return Status::OK;
  }

 private:
  rocksdb::DB* db_;
};

void RunServer(const std::string& address, const std::string& db_path) {
  std::string server_address(address);
  RocksDBServiceImpl service(db_path);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  RunServer("0.0.0.0:50051", "/path/to/rocksdb");
  return 0;
}
