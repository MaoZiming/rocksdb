#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <grpcpp/grpcpp.h>
#include <myproto/cache_service.grpc.pb.h>
#include <myproto/cache_service.pb.h>
#include <myproto/db_service.grpc.pb.h>
#include <myproto/db_service.pb.h>

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

class CacheClient {
 public:
  explicit CacheClient(std::shared_ptr<Channel> channel);
  std::string Get(const std::string &key);
  bool Set(const std::string &key, const std::string &value, int ttl = 0);
  bool SetTTL(const int32_t &ttl);
  float GetMR(void);
  bool Invalidate(const std::string &key, TimeStamp *load);
  bool Update(const std::string &key, const std::string &value,
              TimeStamp *load);
  void AsyncUpdate(const std::string &key, const std::string &value,
                   TimeStamp *load);

 private:
  std::unique_ptr<CacheService::Stub> stub_;
  grpc::CompletionQueue cq_;  // Add this line if cq_ is a member
};

#define USE_STATIC_VALUE
// #define USE_COMPUTATION

#ifdef USE_STATIC_VALUE
const int C_I = 10;
const int C_U = 46;
const int C_M = C_I + C_U;
#endif

const int TTL_EW = -2;
const int INVALIDATE_EW = -3;
const int UPDATE_EW = -4;

#endif  // CLIENT_HPP