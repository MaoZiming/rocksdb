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
const int MAX_CONCURRENT_RPCS = 10000;

TimeStamp GetTimestamp();

class CacheClient {
 public:
  CacheClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(CacheService::NewStub(channel)) {
    // Start the completion queue thread
    cq_thread_ = std::thread(&CacheClient::AsyncCompleteRpc, this);
  }

  ~CacheClient() {
    // Shutdown the completion queue and join the thread
    cq_.Shutdown();
    cq_thread_.join();
  }

  // Asynchronous Get method returning a future
  std::future<std::string> GetAsync(const std::string &key) {
    ++current_rpcs;

    // Build the request
    CacheGetRequest request;
    request.set_key(key);

    // Call object to store RPC data
    AsyncClientCall *call = new AsyncClientCall;
    call->call_type = AsyncClientCall::CallType::GET;
    call->key = key;
    call->get_promise = std::make_shared<std::promise<std::string>>();

    // Get the future from the promise
    std::future<std::string> result_future = call->get_promise->get_future();

    // Start the asynchronous RPC
    call->get_response_reader = stub_->AsyncGet(&call->context, request, &cq_);
    call->get_response_reader->Finish(&call->get_reply, &call->status,
                                      (void *)call);

    return result_future;
  }

  // Asynchronous Set method returning a future
  std::future<bool> SetAsync(const std::string &key, const std::string &value,
                             int ttl) {
    ++current_rpcs;

    // Build the request
    CacheSetRequest request;
    request.set_key(key);
    request.set_value(value);
    request.set_ttl(ttl);

    // Call object to store RPC data
    AsyncClientCall *call = new AsyncClientCall;
    call->call_type = AsyncClientCall::CallType::SET;
    call->key = key;
    call->set_promise = std::make_shared<std::promise<bool>>();

    // Get the future from the promise
    std::future<bool> result_future = call->set_promise->get_future();

    // Start the asynchronous RPC
    call->set_response_reader = stub_->AsyncSet(&call->context, request, &cq_);
    call->set_response_reader->Finish(&call->set_reply, &call->status,
                                      (void *)call);

    return result_future;
  }

  // Asynchronous Invalidate method returning a future
  std::future<bool> InvalidateAsync(const std::string &key) {
    ++current_rpcs;

    // Build the request
    CacheInvalidateRequest request;
    request.set_key(key);

    // std::cerr << "before AsyncClientCall *call" << std::endl;

    // Call object to store RPC data
    AsyncClientCall *call = new AsyncClientCall;
    call->call_type = AsyncClientCall::CallType::INVALIDATE;
    call->key = key;
    call->invalidate_promise = std::make_shared<std::promise<bool>>();

    // Get the future from the promise
    // std::cerr << "before call->invalidate_promise->get_future()" <<
    // std::endl;

    std::future<bool> result_future = call->invalidate_promise->get_future();

    // Start the asynchronous RPC
    call->invalidate_response_reader =
        stub_->AsyncInvalidate(&call->context, request, &cq_);
    call->invalidate_response_reader->Finish(&call->invalidate_reply,
                                             &call->status, (void *)call);

    // std::cout << "InvalidateAsync: finishes" << std::endl;

    return result_future;
  }

  // Asynchronous Update method returning a future
  std::future<bool> UpdateAsync(const std::string &key,
                                const std::string &value, int ttl) {
    // std::cout << "UpdateAsync: before cv" << current_rpcs << std::endl;

    ++current_rpcs;

    // Build the request
    CacheUpdateRequest request;
    request.set_key(key);
    request.set_value(value);

    // Call object to store RPC data
    AsyncClientCall *call = new AsyncClientCall;
    call->call_type = AsyncClientCall::CallType::UPDATE;
    call->key = key;
    call->update_promise = std::make_shared<std::promise<bool>>();

    // Get the future from the promise
    std::future<bool> result_future = call->update_promise->get_future();

    // Start the asynchronous RPC
    call->update_response_reader =
        stub_->AsyncUpdate(&call->context, request, &cq_);
    call->update_response_reader->Finish(&call->update_reply, &call->status,
                                         (void *)call);

    return result_future;
  }

  // Asynchronous SetTTL method returning a future
  std::future<bool> SetTTLAsync(int32_t ttl) {
    ++current_rpcs;

    // Build the request
    CacheSetTTLRequest request;
    request.set_ttl(ttl);

    // Call object to store RPC data
    AsyncClientCall *call = new AsyncClientCall;
    call->call_type = AsyncClientCall::CallType::SETTTL;
    call->set_ttl_promise = std::make_shared<std::promise<bool>>();

    // Get the future from the promise
    std::future<bool> result_future = call->set_ttl_promise->get_future();

    // Start the asynchronous RPC
    call->set_ttl_response_reader =
        stub_->AsyncSetTTL(&call->context, request, &cq_);
    call->set_ttl_response_reader->Finish(&call->set_ttl_reply, &call->status,
                                          (void *)call);

    return result_future;
  }

  // Asynchronous GetMR method returning a future
  std::future<float> GetMRAsync() {
    ++current_rpcs;

    // Build the request
    CacheGetMRRequest request;

    // Call object to store RPC data
    AsyncClientCall *call = new AsyncClientCall;
    call->call_type = AsyncClientCall::CallType::GETMR;
    call->get_mr_promise = std::make_shared<std::promise<float>>();

    // Get the future from the promise
    std::future<float> result_future = call->get_mr_promise->get_future();

    // Start the asynchronous RPC
    call->get_mr_response_reader =
        stub_->AsyncGetMR(&call->context, request, &cq_);
    call->get_mr_response_reader->Finish(&call->get_mr_reply, &call->status,
                                         (void *)call);

    return result_future;
  }

  // Synchronous Get method that waits for the result
  std::string Get(const std::string &key) {
    try {
      std::future<std::string> result_future = GetAsync(key);
      return result_future.get();  // Wait for the result
    } catch (const std::exception &e) {
      std::cerr << "Get failed: " << e.what() << std::endl;
      return "";
    }
  }

  // Synchronous Set method that waits for the result
  bool Set(const std::string &key, const std::string &value, int ttl) {
    try {
      std::future<bool> result_future = SetAsync(key, value, ttl);
      return result_future.get();  // Wait for the result
    } catch (const std::exception &e) {
      std::cerr << "Set failed: " << e.what() << std::endl;
      return false;
    }
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

  // Synchronous SetTTL method that waits for the result
  bool SetTTL(int32_t ttl) {
    try {
      std::future<bool> result_future = SetTTLAsync(ttl);
      return result_future.get();  // Wait for the result
    } catch (const std::exception &e) {
      std::cerr << "SetTTL failed: " << e.what() << std::endl;
      return false;
    }
  }

  // Synchronous GetMR method that waits for the result
  float GetMR() {
    try {
      std::future<float> result_future = GetMRAsync();
      return result_future.get();  // Wait for the result
    } catch (const std::exception &e) {
      std::cerr << "GetMR failed: " << e.what() << std::endl;
      return -1.0;
    }
  }

  int get_current_rpcs() { return current_rpcs.load(); }

 private:
  // Struct to keep state and data information for the asynchronous calls
  struct AsyncClientCall {
    enum class CallType { GET, SET, INVALIDATE, UPDATE, SETTTL, GETMR };

    CallType call_type;
    std::string key;

    // For GetAsync
    CacheGetResponse get_reply;
    std::unique_ptr<grpc::ClientAsyncResponseReader<CacheGetResponse>>
        get_response_reader;
    std::shared_ptr<std::promise<std::string>> get_promise;

    // For SetAsync
    CacheSetResponse set_reply;
    std::unique_ptr<grpc::ClientAsyncResponseReader<CacheSetResponse>>
        set_response_reader;
    std::shared_ptr<std::promise<bool>> set_promise;

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

    // For SetTTLAsync
    CacheSetTTLResponse set_ttl_reply;
    std::unique_ptr<grpc::ClientAsyncResponseReader<CacheSetTTLResponse>>
        set_ttl_response_reader;
    std::shared_ptr<std::promise<bool>> set_ttl_promise;

    // For GetMRAsync
    CacheGetMRResponse get_mr_reply;
    std::unique_ptr<grpc::ClientAsyncResponseReader<CacheGetMRResponse>>
        get_mr_response_reader;
    std::shared_ptr<std::promise<float>> get_mr_promise;

    grpc::ClientContext context;
    grpc::Status status;
  };

  std::atomic<int> current_rpcs{0};
  const int MAX_CONCURRENT_RPCS = 100;
  grpc::CompletionQueue cq_;
  std::thread cq_thread_;
  // std::condition_variable cv_;
  // std::mutex mutex_;

  std::unique_ptr<CacheService::Stub> stub_;

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
            case AsyncClientCall::CallType::GET:
              call->get_promise->set_value(call->get_reply.value());
              break;
            case AsyncClientCall::CallType::SET:
              call->set_promise->set_value(call->set_reply.success());
              break;
            case AsyncClientCall::CallType::INVALIDATE:
              call->invalidate_promise->set_value(
                  call->invalidate_reply.success());

              break;
            case AsyncClientCall::CallType::UPDATE:
              call->update_promise->set_value(call->update_reply.success());
              break;
            case AsyncClientCall::CallType::SETTTL:
              call->set_ttl_promise->set_value(call->set_ttl_reply.success());
              break;
            case AsyncClientCall::CallType::GETMR:
              call->get_mr_promise->set_value(call->get_mr_reply.mr());
              break;
          }
        } else {
          std::cerr << "RPC failed for key: " << call->key << std::endl;
          switch (call->call_type) {
            case AsyncClientCall::CallType::GET:
              call->get_promise->set_value(
                  "");  // Return empty string on failure
              break;
            case AsyncClientCall::CallType::SET:
              call->set_promise->set_value(false);  // Return false on failure
              break;
            case AsyncClientCall::CallType::INVALIDATE:
              call->invalidate_promise->set_value(
                  false);  // Return false on failure
              break;
            case AsyncClientCall::CallType::UPDATE:
              call->update_promise->set_value(
                  false);  // Return false on failure
              break;
            case AsyncClientCall::CallType::SETTTL:
              call->set_ttl_promise->set_value(
                  false);  // Return false on failure
              break;
            case AsyncClientCall::CallType::GETMR:
              call->get_mr_promise->set_value(-1.0f);  // Return -1.0 on failure
              break;
          }
        }

        // Decrement the active RPC count and notify
        // {
        // std::lock_guard<std::mutex> lock(mutex_);
        // {
        // std::lock_guard<std::mutex> lock(mutex_);
        --current_rpcs;
        //   cv_.notify_one();  // Notify one waiting thread to proceed
        // }
        // std::cout << "Decremented rc: " << current_rpcs << std::endl;
        // }
        // cv_.notify_one();
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