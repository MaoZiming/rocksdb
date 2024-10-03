#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <grpcpp/grpcpp.h>
#include <myproto/cache_service.grpc.pb.h>
#include <myproto/cache_service.pb.h>
#include <myproto/db_service.grpc.pb.h>
#include <myproto/db_service.pb.h>

#include <atomic>
#include <future>
#include <iostream>
#include <memory>
#include <string>

using grpc::Channel;
using grpc::ClientContext;
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

typedef unsigned long long int TimeStamp;

TimeStamp GetTimestamp();

#define USE_RPC_LIMIT

#ifdef USE_RPC_LIMIT
const int MAX_CONCURRENT_RPCS = 1000;
#endif

class CacheClient {
 public:
  CacheClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(CacheService::NewStub(channel)) {
    // Start the completion queue thread
    cq_thread_ = std::thread(&CacheClient::AsyncCompleteRpc, this);
  }

  CacheClient(std::vector<std::string> cache_addresses) {
    std::cout << "Created " << cache_addresses.size() << " CacheClient stubs"
              << std::endl;

    for (auto cache_address : cache_addresses) {
      auto channel = grpc::CreateChannel(cache_address,
                                         grpc::InsecureChannelCredentials());
      stubs_.push_back(CacheService::NewStub(channel));
    }
    cq_thread_ = std::thread(&CacheClient::AsyncCompleteRpc, this);
  }

  ~CacheClient() {
    // Shutdown the completion queue and join the thread
    cq_.Shutdown();
    cq_thread_.join();
  }

  int get_current_rpcs() { return current_rpcs.load(); }

  CacheService::Stub *get_stub(const std::string &key) {
    if (stub_) {
      // Return stub_ if it exists
      return stub_.get();
    } else if (!stubs_.empty()) {
      // Hash the key and use the result to select a stub from stubs_
      std::hash<std::string> hash_fn;
      size_t hash_value = hash_fn(key);
      size_t stub_index = hash_value % stubs_.size();
      return stubs_[stub_index].get();
    }

    // Handle the case where both stub_ and stubs_ are empty
    std::cerr << "Error: No stub available!" << std::endl;
    return nullptr;
  }

  // Asynchronous Invalidate method returning a future
  std::future<bool> InvalidateAsync(const std::string &key) {
    {
#ifdef USE_RPC_LIMIT
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return current_rpcs < MAX_CONCURRENT_RPCS; });
#endif
      ++current_rpcs;
    }

    // Build the request
    CacheInvalidateRequest request;
    request.set_key(key);

    // Call object to store RPC data
    AsyncClientCall *call = new AsyncClientCall;
    call->call_type = AsyncClientCall::CallType::INVALIDATE;
    call->key = key;
    call->invalidate_promise = std::make_shared<std::promise<bool>>();
    // call->start_time = std::chrono::steady_clock::now();

    // Get the future from the promise
    std::future<bool> result_future = call->invalidate_promise->get_future();

    // Start the asynchronous RPC
    call->invalidate_response_reader =
        get_stub(key)->AsyncInvalidate(&call->context, request, &cq_);
    call->invalidate_response_reader->Finish(&call->invalidate_reply,
                                             &call->status, (void *)call);

    return result_future;
  }

  // Asynchronous Update method returning a future
  std::future<bool> UpdateAsync(const std::string &key,
                                const std::string &value, int ttl) {
    {
#ifdef USE_RPC_LIMIT
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return current_rpcs < MAX_CONCURRENT_RPCS; });
#endif
      ++current_rpcs;
    }

    // Build the request
    CacheUpdateRequest request;
    request.set_key(key);
    request.set_value(value);

    // Call object to store RPC data
    AsyncClientCall *call = new AsyncClientCall;
    call->call_type = AsyncClientCall::CallType::UPDATE;
    call->key = key;
    call->update_promise = std::make_shared<std::promise<bool>>();
    // call->start_time = std::chrono::steady_clock::now();

    // Get the future from the promise
    std::future<bool> result_future = call->update_promise->get_future();

    // Start the asynchronous RPC
    call->update_response_reader =
        get_stub(key)->AsyncUpdate(&call->context, request, &cq_);
    call->update_response_reader->Finish(&call->update_reply, &call->status,
                                         (void *)call);

    return result_future;
  }

  // Synchronous Invalidate method that waits for the result
  // bool Invalidate(const std::string &key) {
  //   try {
  //     std::future<bool> result_future = InvalidateAsync(key);
  //     return result_future.get();  // Wait for the result
  //   } catch (const std::exception &e) {
  //     std::cerr << "Invalidate failed: " << e.what() << std::endl;
  //     return false;
  //   }
  // }

  // Synchronous Update method that waits for the result
  // bool Update(const std::string &key, const std::string &value, int ttl) {
  //   try {
  //     std::future<bool> result_future = UpdateAsync(key, value, ttl);
  //     return result_future.get();  // Wait for the result
  //   } catch (const std::exception &e) {
  //     std::cerr << "Update failed: " << e.what() << std::endl;
  //     return false;
  //   }
  // }

 private:
  // Struct to keep state and data information for the asynchronous calls
  struct AsyncClientCall {
    enum class CallType { GET, SET, INVALIDATE, UPDATE, SETTTL, GETMR };

    CallType call_type;
    std::string key;

    // For InvalidateAsync
    CacheInvalidateResponse invalidate_reply;
    std::unique_ptr<grpc::ClientAsyncResponseReader<CacheInvalidateResponse>>
        invalidate_response_reader;
    std::shared_ptr<std::promise<bool>> invalidate_promise;

    // For UpdateAsync
    CacheUpdateResponse update_reply;
    std::unique_ptr<grpc::ClientAsyncResponseReader<CacheUpdateResponse>>
        update_response_reader;
    std::shared_ptr<std::promise<bool>> update_promise;
    grpc::ClientContext context;
    grpc::Status status;
  };

#ifdef USE_RPC_LIMIT
  std::mutex mutex_;
  std::condition_variable cv_;
#endif
  std::atomic<int> current_rpcs{0};
  grpc::CompletionQueue cq_;
  std::thread cq_thread_;
  // std::condition_variable cv_;
  // std::mutex mutex_;

  std::unique_ptr<CacheService::Stub> stub_;
  std::vector<std::unique_ptr<CacheService::Stub>> stubs_;

  void AsyncCompleteRpc() {
    std::vector<std::pair<void *, bool>> events;  // To hold batched events
    const int batch_size = 10;  // Adjust this based on experimentation
    void *got_tag;
    bool ok = false;

    while (true) {
      // Batch collection logic
      events.clear();
      for (int i = 0; i < batch_size; ++i) {
        if (!cq_.Next(&got_tag, &ok)) {
          break;  // Break if no more events
        }
        events.emplace_back(got_tag, ok);
      }

      if (events.empty()) {
        break;  // No events in queue, exit the loop
      }

      // Process batched events
      for (const auto &event : events) {
        got_tag = event.first;
        ok = event.second;

        AsyncClientCall *call = static_cast<AsyncClientCall *>(got_tag);
        if (call->status.ok()) {
          // std::cout << "success!" << std::endl;
          switch (call->call_type) {
            case AsyncClientCall::CallType::INVALIDATE:
              call->invalidate_promise->set_value(
                  call->invalidate_reply.success());
              break;
            case AsyncClientCall::CallType::UPDATE:
              call->update_promise->set_value(call->update_reply.success());
              break;
          }
        } else {
          std::cerr << "RPC failed for key: " << call->key << std::endl;
          switch (call->call_type) {
            case AsyncClientCall::CallType::INVALIDATE:
              call->invalidate_promise->set_value(
                  false);  // Return false on failure
              break;
            case AsyncClientCall::CallType::UPDATE:
              call->update_promise->set_value(
                  false);  // Return false on failure
              break;
          }
        }

        {
#ifdef USE_RPC_LIMIT

          std::lock_guard<std::mutex> lock(mutex_);
#endif
          --current_rpcs;
        }
#ifdef USE_RPC_LIMIT

        cv_.notify_one();
#endif

        delete call;
      }
    }
  }
};
#define USE_STATIC_VALUE
// #define USE_COMPUTATION

#ifdef USE_STATIC_VALUE
const int C_I = 10;               // Invalidate
const int C_U = 46;               // Update
const int C_D = 500;              // SSD read
const int C_M = C_I + C_U + C_D;  // Miss
#endif

const int TTL_EW = -2;
const int INVALIDATE_EW = -3;
const int UPDATE_EW = -4;

#endif  // CLIENT_HPP