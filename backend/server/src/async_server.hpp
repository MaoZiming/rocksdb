#pragma once

#include <grpcpp/grpcpp.h>
#include <myproto/db_service.grpc.pb.h>
#include <myproto/db_service.pb.h>
#include <rocksdb/db.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "client.hpp"
#include "load_tracker.hpp"
#include "thread_pool.hpp"
using grpc::Channel;
using grpc::ClientContext;
using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

using freshCache::CacheGetMRRequest;
using freshCache::CacheGetMRResponse;
using freshCache::CacheGetRequest;
using freshCache::CacheGetResponse;
using freshCache::CacheInvalidateRequest;
using freshCache::CacheInvalidateResponse;
using freshCache::CacheService;
using freshCache::CacheSetRequest;
using freshCache::CacheSetResponse;
using freshCache::CacheSetTTLRequest;
using freshCache::CacheSetTTLResponse;
using freshCache::CacheUpdateRequest;
using freshCache::CacheUpdateResponse;
using freshCache::DBDeleteRequest;
using freshCache::DBDeleteResponse;
using freshCache::DBGetLoadRequest;
using freshCache::DBGetLoadResponse;
using freshCache::DBGetReadCountRequest;
using freshCache::DBGetReadCountResponse;
using freshCache::DBGetRequest;
using freshCache::DBGetResponse;
using freshCache::DBGetWriteCountRequest;
using freshCache::DBGetWriteCountResponse;
using freshCache::DBPutRequest;
using freshCache::DBPutResponse;
using freshCache::DBService;
using freshCache::DBStartRecordRequest;
using freshCache::DBStartRecordResponse;

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "client.hpp"  // Assuming CacheClient is defined here
const int STALENESS_BOUND_IN_MS = 0;

class AsyncServer;  // Forward declaration
extern double get_cpu_load();

// Base class for CallData
class CallDataBase {
 public:
  virtual void Proceed() = 0;
  virtual ~CallDataBase() {}
};

class CallDataPut : public CallDataBase {
 public:
  CallDataPut(DBService::AsyncService *service, ServerCompletionQueue *cq,
              AsyncServer *server);
  void Proceed() override;

 private:
  void HandlePut();

  // Class members
  DBService::AsyncService *service_;
  ServerCompletionQueue *cq_;
  ServerContext ctx_;
  DBPutRequest put_request_;
  DBPutResponse put_response_;
  ServerAsyncResponseWriter<DBPutResponse> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;

  AsyncServer *server_;
};

class CallDataGet : public CallDataBase {
 public:
  CallDataGet(DBService::AsyncService *service, ServerCompletionQueue *cq,
              AsyncServer *server);
  void Proceed() override;

 private:
  void HandleGet();

  // Class members
  DBService::AsyncService *service_;
  ServerCompletionQueue *cq_;
  ServerContext ctx_;
  DBGetRequest get_request_;
  DBGetResponse get_response_;
  ServerAsyncResponseWriter<DBGetResponse> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;

  AsyncServer *server_;
};

class CallDataDelete : public CallDataBase {
 public:
  CallDataDelete(DBService::AsyncService *service, ServerCompletionQueue *cq,
                 AsyncServer *server);
  void Proceed() override;

 private:
  void HandleDelete();

  // Class members
  DBService::AsyncService *service_;
  ServerCompletionQueue *cq_;
  ServerContext ctx_;
  DBDeleteRequest delete_request_;
  DBDeleteResponse delete_response_;
  ServerAsyncResponseWriter<DBDeleteResponse> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;

  AsyncServer *server_;
};

class CallDataStartRecord : public CallDataBase {
 public:
  CallDataStartRecord(DBService::AsyncService *service,
                      ServerCompletionQueue *cq, AsyncServer *server);
  void Proceed() override;

 private:
  void HandleStartRecord();

  // Class members
  DBService::AsyncService *service_;
  ServerCompletionQueue *cq_;
  ServerContext ctx_;
  DBStartRecordRequest request_;
  DBStartRecordResponse response_;
  ServerAsyncResponseWriter<DBStartRecordResponse> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;

  AsyncServer *server_;
};

class CallDataGetLoad : public CallDataBase {
 public:
  CallDataGetLoad(DBService::AsyncService *service, ServerCompletionQueue *cq,
                  AsyncServer *server);
  void Proceed() override;

 private:
  void HandleGetLoad();

  // Class members
  DBService::AsyncService *service_;
  ServerCompletionQueue *cq_;
  ServerContext ctx_;
  DBGetLoadRequest getload_request_;
  DBGetLoadResponse getload_response_;
  ServerAsyncResponseWriter<DBGetLoadResponse> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;

  AsyncServer *server_;
};

class CallDataGetReadCount : public CallDataBase {
 public:
  CallDataGetReadCount(DBService::AsyncService *service,
                       ServerCompletionQueue *cq, AsyncServer *server);
  void Proceed() override;

 private:
  void HandleGetReadCount();

  // Class members
  DBService::AsyncService *service_;
  ServerCompletionQueue *cq_;
  ServerContext ctx_;
  DBGetReadCountRequest request_;
  DBGetReadCountResponse response_;
  ServerAsyncResponseWriter<DBGetReadCountResponse> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;

  AsyncServer *server_;
};

class CallDataGetWriteCount : public CallDataBase {
 public:
  CallDataGetWriteCount(DBService::AsyncService *service,
                        ServerCompletionQueue *cq, AsyncServer *server);
  void Proceed() override;

 private:
  void HandleGetWriteCount();

  // Class members
  DBService::AsyncService *service_;
  ServerCompletionQueue *cq_;
  ServerContext ctx_;
  DBGetWriteCountRequest request_;
  DBGetWriteCountResponse response_;
  ServerAsyncResponseWriter<DBGetWriteCountResponse> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;

  AsyncServer *server_;
};

class AsyncServer {
 public:
  AsyncServer(const std::string &address, const std::string &db_path,
              CacheClient *cache_client)
      : server_address_(address),
        db_path_(db_path),
        cache_client_(cache_client),
        get_count_(0),
        set_count_(0),
        load_(0),
        thread_pool_(
            std::thread::hardware_concurrency()) {  // Initialize thread pool

    // Open RocksDB
    rocksdb::Options options;
    options.create_if_missing = true;
    options.disable_auto_compactions = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path_, &db_);
    if (!status.ok()) {
      std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
      exit(1);
    }
  }

  ~AsyncServer() {
    server_->Shutdown();
    cq_->Shutdown();
    delete db_;
  }

  void Run() {
    ServerBuilder builder;
    builder.AddListeningPort(server_address_,
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    cq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();

    std::cout << "Server listening on " << server_address_ << std::endl;

    // Start handling RPCs
    std::thread rpc_thread(&AsyncServer::HandleRpcs, this);

    // Start the periodic task
    std::atomic<bool> running(true);
    std::thread periodic_thread(&AsyncServer::RunPeriodicTask, this,
                                std::ref(running));

    rpc_thread.join();
    running = false;
    periodic_thread.join();
  }

  // Method to check buffer
  void CheckBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &pair : bufferedWrites_) {
      const std::string &key = pair.first;
      const std::string &value = pair.second.first;
      float ew = pair.second.second;
      InvalidateOrUpdate(key, value, ew);
    }
    bufferedWrites_.clear();
  }

  double calculate_cost_difference(double ew, const std::string &key,
                                   const std::string &value) {
    const int KB = 1000;

    // Calculate costs using double
    double update_cost =
        ew * C_U;                  // * (value.size() + key.size()) / (10 * KB);
    double invalidate_cost = C_I;  // * key.size() / (10 * KB);
    double miss_cost = (C_U + C_I)  // * (value.size() + key.size()) / (10 * KB)
                       + C_D;

    // New calculation.
    update_cost = ew * (value.size() + key.size());
    invalidate_cost = key.size();
    miss_cost = value.size() + key.size() + key.size();

    // Print the costs for debugging
    /*
    std::cout << "ew: " << ew << ", key_size: " << key.size()
              << ", value_size: " << value.size() << std::endl;
    std::cout << "Update: " << update_cost << std::endl;
    std::cout << "Invalidate: " << invalidate_cost << std::endl;
    std::cout << "Miss: " << miss_cost << std::endl;
    */
    // Return the difference between invalidate + miss cost and update cost
    return (invalidate_cost + miss_cost) - update_cost;
  }

  bool is_invalidate_cheaper(double ew, const std::string &key,
                             const std::string &value) {
    double cost_diff = calculate_cost_difference(ew, key, value);

    // Return true if invalidate is cheaper, false otherwise
    return cost_diff < 0;
  }

  // Other methods
  void InvalidateOrUpdate(const std::string &key, const std::string &value,
                          float ew) {
    if (ew == INVALIDATE_EW) {
      Invalidate(key);
    } else if (ew == TTL_EW) {
      return;
    } else if (ew == UPDATE_EW) {
      Update(key, value);
    } else {
      // Smart policy
      assert(ew >= 0 || ew == -1);
      // std::cout << "Invalidation Count: " << invalidation_count_ <<
      // std::endl;
      if (!check_is_in_cache(key)) {
        // std::cout << "Not in cache!" << key << ", ew: " << ew << std::endl;
        return;
      }

      if (get_cpu_load() > 0.8) {
        /* Always Invalidate when high load.*/
        Invalidate(key);
      }
      /*
      else if (get_cpu_load() < 0.2) {
        // Invalidate when low load
      Update(key, value);
      }
      */

      else if (ew == -1) {
        // std::cout << "Invalidate!" << key << ", ew: " << ew << std::endl;
        // One hit wonders!
        /*
        if (invalidation_count_ &&
            static_cast<double>(miss_count_due_to_invalidates_) /
                    invalidation_count_ >
                0.3)
          Update(key, value);
        else
          Invalidate(key);
          */
        Invalidate(key);
      }

      else {
#ifdef USE_STATIC_VALUE
        // if (ew * C_U > C_I + C_M) {
        if (is_invalidate_cheaper(ew, key, value)) {
          // std::cout << "Invalidate!" << key << ", ew: " << ew << std::endl;
          Invalidate(key);
        } else {
          // std::cout << "Update!" << key << ", ew: " << ew << std::endl;
          Update(key, value);
        }
#else
        if (ew > 0 && ew <= 1) {
          Update(key, value);
        } else if (ew == -1) {
          Invalidate(key);
        }
#endif
      }
    }
  }

  std::shared_mutex invalidate_mutex_;

  bool check_is_in_cache(const std::string &key) {
    std::lock_guard<std::shared_mutex> lock(invalidate_mutex_);
    auto it = is_key_invalidated_.find(key);
    if (it == is_key_invalidated_.end()) {
      return true;
    }
    return !it->second;
  }

  void Invalidate(const std::string &key) {
    // Offload to a new thread
    thread_pool_.enqueue([this, key]() {
      // Perform async invalidation
      cache_client_->InvalidateAsync(key);

      // Update internal state after invalidation
      {
        std::lock_guard<std::mutex> lock(load_mutex_);
        set_invalidate(key);
        load_ += C_I;
        invalidation_count_++;
      }
    });
  }

  void set_miss(const std::string &key) {
    std::unique_lock<std::shared_mutex> lock(invalidate_mutex_);
    if (is_key_invalidated_[key]) miss_count_due_to_invalidates_++;
    if (is_key_invalidated_.size() >= is_key_invalidated_size_)
      evict_one_invalidate();
    is_key_invalidated_[key] = false;
  }

  void set_invalidate(const std::string &key) {
    std::unique_lock<std::shared_mutex> lock(invalidate_mutex_);
    if (is_key_invalidated_.size() >= is_key_invalidated_size_)
      evict_one_invalidate();
    is_key_invalidated_[key] = true;
  }

  void evict_one_invalidate() {
    if (!is_key_invalidated_.empty()) {
      auto it = is_key_invalidated_.begin();  // Get an arbitrary element
      is_key_invalidated_.erase(it);          // Remove it from the map
    }
  }

  void Update(const std::string &key, const std::string &value) {
    thread_pool_.enqueue([this, key, value]() {
      cache_client_->UpdateAsync(key, value, 0);
      {
        std::lock_guard<std::mutex> lock(load_mutex_);
        load_ += C_U;
      }
    });
  }

  void WriteBuffer(const std::string &key, const std::string &value, float ew) {
    std::lock_guard<std::mutex> lock(mutex_);
    bufferedWrites_[key] = std::make_pair(value, ew);
  }

  // Getter methods for shared resources
  rocksdb::DB *db() { return db_; }
  CacheClient *cache_client() { return cache_client_; }
  std::atomic<int> &get_count() { return get_count_; }
  std::atomic<int> &set_count() { return set_count_; }
  TimeStamp &load() { return load_; }
  std::unordered_map<std::string, bool> &is_key_invalidated() {
    return is_key_invalidated_;
  }
  std::mutex &mutex() { return mutex_; }

  int getActiveConnections() { return active_connections.load(); }

 private:
  std::atomic<int> active_connections{0};
  ThreadPool thread_pool_;

  void HandleRpcs() {
    // Spawn new CallData instances to serve new clients
    new CallDataPut(&service_, cq_.get(), this);
    new CallDataGet(&service_, cq_.get(), this);
    new CallDataDelete(&service_, cq_.get(), this);
    new CallDataGetLoad(&service_, cq_.get(), this);
    new CallDataGetReadCount(&service_, cq_.get(), this);
    new CallDataGetWriteCount(&service_, cq_.get(), this);
    new CallDataStartRecord(&service_, cq_.get(), this);

    /*
    void *tag;  // uniquely identifies a request
    bool ok;
    while (cq_->Next(&tag, &ok)) {
      if (!ok) {
        // Handle failure or completion error
        continue;
      }
      static_cast<CallDataBase *>(tag)->Proceed();
    }
    */

    std::vector<std::thread> workers;
    int num_threads =
        std::thread::hardware_concurrency();  // Use all available cores

    // Each thread processes events from the CompletionQueue
    for (int i = 0; i < num_threads; ++i) {
      workers.emplace_back([this]() {
        void *tag;
        bool ok;
        while (cq_->Next(&tag, &ok)) {
          // std::cout << "Got a new message!" << std::endl;
          if (ok) {
            ++active_connections;
            static_cast<CallDataBase *>(tag)->Proceed();  // Process the event
          }

          --active_connections;  // Once the request is done
          // std::cout << "Finish that message! " << std::endl;
        }
      });
    }

    // Join threads before exiting
    for (auto &worker : workers) {
      worker.join();
    }
  }

  void RunPeriodicTask(std::atomic<bool> &running) {
    if (STALENESS_BOUND_IN_MS == 0) return;
    while (running) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(STALENESS_BOUND_IN_MS));
      CheckBuffer();
    }
  }

  // Server state
  std::string server_address_;
  std::string db_path_;
  CacheClient *cache_client_;
  std::unique_ptr<ServerCompletionQueue> cq_;
  DBService::AsyncService service_;
  std::unique_ptr<Server> server_;
  int64_t invalidation_count_ = 0;
  int64_t miss_count_due_to_invalidates_ = 0;

  // Shared resources
  rocksdb::DB *db_;
  std::atomic<int> get_count_;
  std::atomic<int> set_count_;
  TimeStamp load_;
  std::unordered_map<std::string, std::pair<std::string, float>>
      bufferedWrites_;
  std::unordered_map<std::string, bool> is_key_invalidated_;
  size_t is_key_invalidated_size_ = 10000000;
  std::mutex mutex_;
  std::mutex load_mutex_;

  friend class CallDataPut;
  friend class CallDataGet;
  friend class CallDataDelete;
  friend class CallDataGetLoad;
  friend class CallDataStartRecord;
  friend class CallDataGetReadCount;
  friend class CallDataGetWriteCount;
};