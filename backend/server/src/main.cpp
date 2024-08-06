#include <grpcpp/grpcpp.h>
#include <myproto/rocksdb_service.grpc.pb.h>
#include <myproto/rocksdb_service.pb.h>
#include <rocksdb/db.h>

#include <iostream>
#include <memory>
#include <string>

using freshCache::DeleteRequest;
using freshCache::DeleteResponse;
using freshCache::GetRequest;
using freshCache::GetResponse;
using freshCache::PutRequest;
using freshCache::PutResponse;
using freshCache::RocksDBService;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

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

  Status Get(ServerContext* context, const GetRequest* request,
             GetResponse* response) override {
    std::string value;
    rocksdb::Status s =
        db_->Get(rocksdb::ReadOptions(), request->key(), &value);
    if (s.ok()) {
      response->set_value(value);
      response->set_found(true);
    } else {
      response->set_value("");
      response->set_found(false);
    }
    return Status::OK;
  }

  Status Delete(ServerContext* context, const DeleteRequest* request,
                DeleteResponse* response) override {
    rocksdb::Status s = db_->Delete(rocksdb::WriteOptions(), request->key());
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
  // Default values
  std::string port = "50051";
  std::string rocksdb_path = "/path/to/rocksdb";

  // Check the number of arguments
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <port> <rocksdb_path>" << std::endl;
    return 1;  // Return with error code
  }

  // Read arguments
  port = argv[1];
  rocksdb_path = argv[2];

  // Construct the server address
  std::string server_address = "0.0.0.0:" + port;

  // Run the server
  RunServer(server_address, rocksdb_path);

  return 0;
}
