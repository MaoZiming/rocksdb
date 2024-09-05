#include <grpcpp/grpcpp.h>
#include <myproto/db_service.grpc.pb.h>
#include <myproto/db_service.pb.h>
#include <rocksdb/db.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "client.hpp"
#include "load_tracker.hpp"

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

const int STALENESS_BOUND = 1;

void computeMatrixMultiplication(int size) {
  std::vector<std::vector<int>> matrix1(size, std::vector<int>(size, 1));
  std::vector<std::vector<int>> matrix2(size, std::vector<int>(size, 2));
  std::vector<std::vector<int>> result(size, std::vector<int>(size, 0));

  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      for (int k = 0; k < size; ++k) {
        result[i][j] += matrix1[i][k] * matrix2[k][j];
      }
    }
  }
}

class DBServiceImpl final : public DBService::Service {
 public:
  DBServiceImpl(const std::string& db_path, CacheClient* cache_client) {
    rocksdb::Options options;
    options.create_if_missing = true;
    options.disable_auto_compactions = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_);
    if (!status.ok()) {
      std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
      exit(1);
    }
    cache_client_ = cache_client;
  }

  ~DBServiceImpl() { delete db_; }

  void invalidate_or_update(std::string key, std::string value, float ew) {
    if (ew == INVALIDATE_EW) {
      invalidate(key);
    } else if (ew == UPDATE_EW) {
      update(key, value);
    } else {
      assert(ew > 0 || ew == -1);
      if (ew == -1) {
        invalidate(key);
      } else {
#ifdef USE_STATIC_VALUE
        if (ew * C_U > C_I + C_M) {
          invalidate(key);
        } else {
          update(key, value);
        }
#else
        if (ew > 0 && ew <= 1) {
          update(key, value);
        } else if (ew == -1) {
          invalidate(key);
        }
#endif
      }
    }
  }

  Status Put(ServerContext* context, const DBPutRequest* request,
             DBPutResponse* response) override {
#ifdef DEBUG
    std::cout << "Put: " << request->key() << ", " << request->value()
              << std::endl;
#endif

    rocksdb::WriteOptions write_options;
    // write_options.disableWAL = true;
    rocksdb::Status s =
        db_->Put(write_options, request->key(), request->value());
    response->set_success(s.ok());
    float ew = request->ew();
#ifdef DEBUG
    std::cout << "ew: " << ew << std::endl;
#endif

    if (ew == TTL_EW) {
      // disabled
      return Status::OK;
    } else {
      if (STALENESS_BOUND == 0)
        invalidate_or_update(request->key(), request->value(), ew);
      else
        write_buffer(request->key(), request->value(), ew);
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

#ifdef USE_COMPUTATION
    // Scale matrix size based on value length
    int matrix_size = std::max(
        10, static_cast<int>(value.size() /
                             100));  // Minimum size 10, scales with value

    computeMatrixMultiplication(matrix_size);
#endif

    TimeStamp end = GetTimestamp();  // End timing

    if (s.ok()) {
      response->set_value(value);
      response->set_found(true);
    } else {
      // std::cout << "key not found! " << request->key() << std::endl;
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
    // Scale matrix size based on value length
    int matrix_size = std::max(
        10, static_cast<int>(value.size() /
                             100));  // Minimum size 10, scales with value

#ifdef USE_COMPUTATION
    computeMatrixMultiplication(matrix_size);
#endif

    cache_client_->Update(key, value, &load_);
  }

  void write_buffer(const std::string key, std::string value, float ew) {
    std::lock_guard<std::mutex> lock(mutex_);
    bufferedWrites_[key] = std::make_pair(value, ew);
  }

  void CheckBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : bufferedWrites_) {
      const std::string& key = pair.first;
      const std::string& value = pair.second.first;
      float ew = pair.second.second;
      invalidate_or_update(key, value, ew);
    }
    bufferedWrites_.clear();
  }

 private:
  rocksdb::DB* db_;
  TimeStamp load_ = 0;
  CacheClient* cache_client_ = nullptr;
  std::unordered_map<std::string, std::pair<std::string, float>>
      bufferedWrites_;
  std::mutex mutex_;
};

void RunPeriodicTask(DBServiceImpl* service, std::atomic<bool>& running) {
  if (STALENESS_BOUND == 0) return;
  while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(STALENESS_BOUND));
    service->CheckBuffer();
  }
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

  std::atomic<bool> running(true);
  std::thread periodic_thread(RunPeriodicTask, &service, std::ref(running));

  std::string timestamp = GetT();

  std::string logFilename =
      "cpu_" + timestamp + ".log";  // Construct the filename
  START_COLLECTION(logFilename);
  server->Wait();
  running = false;
  periodic_thread.join();
  END_COLLECTION();
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
