#include <grpcpp/grpcpp.h>
#include <myproto/db_service.grpc.pb.h>
#include <myproto/db_service.pb.h>
#include <rocksdb/db.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "client.hpp"

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
  DBServiceImpl(const std::string& db_path, CacheClient* cache_client) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_);
    if (!status.ok()) {
      std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
      exit(1);
    }
    cache_client_ = cache_client;
  }

  ~DBServiceImpl() { delete db_; }

  Status Put(ServerContext* context, const DBPutRequest* request,
             DBPutResponse* response) override {
#ifdef DEBUG
    std::cout << "Put: " << request->key() << ", " << request->value()
              << std::endl;
#endif
    rocksdb::Status s =
        db_->Put(rocksdb::WriteOptions(), request->key(), request->value());
    response->set_success(s.ok());

    float ew = request->ew();
#ifdef DEBUG
    std::cout << "ew: " << ew << std::endl;
#endif

    if (ew == TTL_EW) {
      // disabled
      return Status::OK;
    } else if (ew == INVALIDATE_EW) {
      invalidate(request->key());
    } else if (ew == UPDATE_EW) {
      update(request->key(), request->value());
    } else {
      assert(ew > 0 || ew == -1);
      if (ew == -1) {
        invalidate(request->key());
      } else {
#ifdef USE_STATIC_VALUE
        if (ew * C_U > C_I + C_M) {
          invalidate(request->key());
        } else {
          update(request->key(), request->value());
        }
#else
        if (ew > 0 && ew <= 1) {
          update(request->key(), request->value());
        } else if (ew == -1) {
          invalidate(request->key());
        }
#endif
      }
    }
    return Status::OK;
  }

  Status Get(ServerContext* context, const DBGetRequest* request,
             DBGetResponse* response) override {
#ifdef DEBUG
    std::cout << "Get: " << request->key() << std::endl;
#endif
    TimeStamp start = GetTimestamp();  // Start timing
    std::string value;
    rocksdb::Status s =
        db_->Get(rocksdb::ReadOptions(), request->key(), &value);

    TimeStamp end = GetTimestamp();  // End timing

    if (s.ok()) {
      response->set_value(value);
      response->set_found(true);
    } else {
      response->set_value("");
      response->set_found(false);
    }

#ifdef USE_STATIC_VALUE
    load_ += C_M;
#else
    load_ += end - start;  // Store the duration in load_
#endif

#ifdef DEBUG
    std::cout << "Load" << (int)load_ << std::endl;
#endif
    return Status::OK;
  }

  Status Delete(ServerContext* context, const DBDeleteRequest* request,
                DBDeleteResponse* response) override {
#ifdef DEBUG
    std::cout << "Delete: " << request->key() << std::endl;
#endif
    rocksdb::Status s = db_->Delete(rocksdb::WriteOptions(), request->key());
    response->set_success(s.ok());
    return Status::OK;
  }

  Status GetLoad(ServerContext* context, const DBGetLoadRequest* request,
                 DBGetLoadResponse* response) override {
#ifdef DEBUG
    std::cout << "Get Load" << std::endl;
#endif
    response->set_load(load_);
    response->set_success(true);
    // Reset Load.
    load_ = 0;
    return Status::OK;
  }

  void invalidate(const std::string key) {
    cache_client_->Invalidate(key, &load_);
  }
  void update(const std::string key, std::string value) {
    cache_client_->Update(key, value, &load_);
  }

 private:
  rocksdb::DB* db_;
  TimeStamp load_ = 0;
  CacheClient* cache_client_ = nullptr;
};

class LatencyInterceptor : public grpc::experimental::Interceptor {
 public:
  void Intercept(
      grpc::experimental::InterceptorBatchMethods* methods) override {
    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::
                PRE_SEND_INITIAL_METADATA)) {
      start_time_ = std::chrono::high_resolution_clock::now();
      std::cout << "Timer start " << std::endl;
    }

    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_SEND_MESSAGE)) {
      std::cout << "PRE_SEND_MESSAGE hook triggered" << std::endl;
    }

    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::POST_SEND_MESSAGE)) {
      std::cout << "POST_SEND_MESSAGE hook triggered" << std::endl;
    }

    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::PRE_RECV_MESSAGE)) {
      std::cout << "PRE_RECV_MESSAGE hook triggered" << std::endl;
    }

    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::POST_RECV_MESSAGE)) {
      std::cout << "POST_RECV_MESSAGE hook triggered" << std::endl;
    }

    if (methods->QueryInterceptionHookPoint(
            grpc::experimental::InterceptionHookPoints::POST_RECV_STATUS)) {
      auto end_time = std::chrono::high_resolution_clock::now();
      auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                         end_time - start_time_)
                         .count();
      std::cout << "Full Server-Side Latency: " << latency << " microseconds"
                << std::endl;
    }

    methods->Proceed();
  }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
};

class LatencyInterceptorFactory
    : public grpc::experimental::ServerInterceptorFactoryInterface {
 public:
  grpc::experimental::Interceptor* CreateServerInterceptor(
      grpc::experimental::ServerRpcInfo* info) override {
    return new LatencyInterceptor;
  }
};

void RunServerWithInterceptors(const std::string& address,
                               const std::string& db_path,
                               CacheClient* cache_client) {
  std::string server_address(address);
  DBServiceImpl service(db_path, cache_client);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::vector<
      std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>>
      interceptors;
  interceptors.push_back(
      std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>(
          new LatencyInterceptorFactory()));

  builder.experimental().SetInterceptorCreators(std::move(interceptors));

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

void RunServer(const std::string& address, const std::string& db_path,
               CacheClient* cache_client) {
  std::string server_address(address);
  DBServiceImpl service(db_path, cache_client);

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
  CacheClient cache_client(grpc::CreateChannel(
      "10.128.0.34:50051", grpc::InsecureChannelCredentials()));

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
  RunServer(server_address, rocksdb_path, &cache_client);

  return 0;
}
