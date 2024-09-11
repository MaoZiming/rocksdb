#include "client.hpp"

#include <future>
// Implementation of CacheClient methods

TimeStamp GetTimestamp() {
  struct timeval now;
  gettimeofday(&now, nullptr);

  return now.tv_usec + (TimeStamp)now.tv_sec * 1000000;
}

CacheClient::CacheClient(std::shared_ptr<Channel> channel)
    : stub_(CacheService::NewStub(channel)) {}

std::string CacheClient::Get(const std::string &key) {
  CacheGetRequest request;
  request.set_key(key);

  CacheGetResponse response;
  ClientContext context;

  Status status = stub_->Get(&context, request, &response);

  if (status.ok()) {
    if (response.success()) {
      return response.value();
    } else {
      std::cerr << "Key not found." << std::endl;
    }
  } else {
    std::cerr << "RPC failed." << std::endl;
  }

  return "";
}

bool CacheClient::Set(const std::string &key, const std::string &value,
                      int ttl) {
  CacheSetRequest request;
  request.set_key(key);
  request.set_value(value);
  request.set_ttl(ttl);

  CacheSetResponse response;
  ClientContext context;

  Status status = stub_->Set(&context, request, &response);

  if (status.ok()) {
    return response.success();
  } else {
    std::cerr << "RPC failed." << std::endl;
  }

  return false;
}

bool CacheClient::SetTTL(const int32_t &ttl) {
  CacheSetTTLRequest request;
  request.set_ttl(ttl);

  CacheSetTTLResponse response;
  ClientContext context;

  Status status = stub_->SetTTL(&context, request, &response);

  if (status.ok()) {
    return response.success();
  } else {
    std::cerr << "RPC failed." << std::endl;
  }

  return false;
}

float CacheClient::GetMR(void) {
  CacheGetMRRequest request;

  CacheGetMRResponse response;
  ClientContext context;

  Status status = stub_->GetMR(&context, request, &response);

  if (status.ok()) {
    return response.mr();
  } else {
    std::cerr << "RPC failed." << std::endl;
  }

  return -1;
}

bool CacheClient::Invalidate(const std::string &key, TimeStamp *load) {
  // TODO: Use async and return a future.
  CacheInvalidateRequest request;
  CacheInvalidateResponse response;
  ClientContext context;

  TimeStamp start = GetTimestamp();
  request.set_key(key);

#ifdef USE_STATIC_VALUE
  (*load) += C_I;
#else
  (*load) += GetTimestamp() - start;
#endif
  Status status = stub_->Invalidate(&context, request, &response);

  if (status.ok()) {
    return true;
  } else {
    std::cerr << "RPC failed." << std::endl;
  }

  return false;
}

bool CacheClient::Update(const std::string &key, const std::string &value,
                         TimeStamp *load) {
  // TODO: Use async and return a future.
  CacheUpdateRequest request;
  CacheUpdateResponse response;
  ClientContext context;

  TimeStamp start = GetTimestamp();
  request.set_key(key);
  request.set_value(value);

#ifdef USE_STATIC_VALUE
  (*load) += C_U;
#else
  (*load) += GetTimestamp() - start;
#endif
  Status status = stub_->Update(&context, request, &response);

  if (status.ok()) {
    return true;
  } else {
    std::cerr << "RPC failed." << std::endl;
  }

  return false;
}

// BUGGY
void CacheClient::AsyncUpdate(const std::string &key, const std::string &value,
                              TimeStamp *load) {
  // TODO: Use async and return a future.
  CacheUpdateRequest request;
  CacheUpdateResponse response;
  ClientContext context;

  TimeStamp start = GetTimestamp();
  request.set_key(key);
  request.set_value(value);

#ifdef USE_STATIC_VALUE
  (*load) += C_U;
#else
  (*load) += GetTimestamp() - start;
#endif

  stub_->AsyncUpdate(&context, request, &cq_);
  return;
}