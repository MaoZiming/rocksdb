#include <grpcpp/grpcpp.h>
#include <myproto/db_service.grpc.pb.h>
#include <myproto/db_service.pb.h>
#include <rocksdb/db.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

using freshCache::DBDeleteRequest;
using freshCache::DBDeleteResponse;
using freshCache::DBGetLoadRequest;
using freshCache::DBGetLoadResponse;
using freshCache::DBGetRequest;
using freshCache::DBGetResponse;
using freshCache::DBPutRequest;
using freshCache::DBPutResponse;
using freshCache::DBService;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

class DBServiceImpl final : public DBService::Service {
 public:
  DBServiceImpl(const std::string& db_path) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_);
    if (!status.ok()) {
      std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
      exit(1);
    }
  }

  ~DBServiceImpl() { delete db_; }

  Status Put(ServerContext* context, const DBPutRequest* request,
             DBPutResponse* response) override {
    std::cout << "Put: " << request->key() << ", " << request->value()
              << std::endl;
    rocksdb::Status s =
        db_->Put(rocksdb::WriteOptions(), request->key(), request->value());
    response->set_success(s.ok());
    return Status::OK;
  }

  Status Get(ServerContext* context, const DBGetRequest* request,
             DBGetResponse* response) override {
    std::cout << "Get: " << request->key() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();  // Start timing
    std::string value;
    rocksdb::Status s =
        db_->Get(rocksdb::ReadOptions(), request->key(), &value);

    auto end = std::chrono::high_resolution_clock::now();  // End timing
    std::chrono::duration<float> duration = end - start;

    if (s.ok()) {
      response->set_value(value);
      response->set_found(true);
    } else {
      response->set_value("");
      response->set_found(false);
    }

    load_ += duration.count();  // Store the duration in load_

    return Status::OK;
  }

  Status Delete(ServerContext* context, const DBDeleteRequest* request,
                DBDeleteResponse* response) override {
    std::cout << "Delete: " << request->key() << std::endl;
    rocksdb::Status s = db_->Delete(rocksdb::WriteOptions(), request->key());
    response->set_success(s.ok());
    return Status::OK;
  }

  Status GetLoad(ServerContext* context, const DBGetLoadRequest* request,
                 DBGetLoadResponse* response) override {
    std::cout << "Get Load" << std::endl;
    response->set_load(load_);
    response->set_success(true);
    return Status::OK;
  }

 private:
  rocksdb::DB* db_;
  float load_;
};

void RunServer(const std::string& address, const std::string& db_path) {
  std::string server_address(address);
  DBServiceImpl service(db_path);

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
  std::string rocksdb_path = "test.db";

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
