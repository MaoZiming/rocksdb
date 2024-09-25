#include <grpcpp/grpcpp.h>
#include <myproto/db_service.grpc.pb.h>
#include <myproto/db_service.pb.h>
#include <rocksdb/db.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
// #include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "async_server.hpp"
#include "client.hpp"
#include "load_tracker.hpp"

const int KB = 1000;
const int MB = 1000 * KB;

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
using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

std::string log_path = "test.log";

// CallData classes for each RPC method

// Now we can define the constructors and Proceed methods for each CallData
// class

// CallDataPut Implementation
CallDataPut::CallDataPut(DBService::AsyncService *service,
                         ServerCompletionQueue *cq, AsyncServer *server)
    : service_(service),
      cq_(cq),
      server_(server),
      responder_(&ctx_),
      status_(CREATE) {
  Proceed();
}

void CallDataPut::Proceed() {
  if (status_ == CREATE) {
    status_ = PROCESS;
    // Request a new Put RPC
    // std::cout << "DB before RequestPut" << std::endl;
    service_->RequestPut(&ctx_, &put_request_, &responder_, cq_, cq_, this);
    // std::cout << "DB after RequestPut" << std::endl;
  } else if (status_ == PROCESS) {
    // Spawn a new CallDataPut instance to serve new clients while we process
    // the current one
    new CallDataPut(service_, cq_, server_);

    // Process the request
    HandlePut();
    // std::cout << "CallDataPut HandlePut() finishes!" << std::endl;

    status_ = FINISH;
    responder_.Finish(put_response_, Status::OK, this);
    // std::cout << "CallDataPut finish" << std::endl;

    // std::cout << "responder finishes!" << std::endl;
  } else {
    // Clean up and delete this instance
    // std::cout << "CallDataPut finishes!" << std::endl;
    delete this;
  }
}

void CallDataPut::HandlePut() {
  // std::cout << "Put: " << put_request_.key() << ", " << put_request_.value()
  //           << std::endl;
  // std::cout << "CallDataPut start" << std::endl;
  rocksdb::WriteOptions write_options;
  rocksdb::Status s = server_->db()->Put(write_options, put_request_.key(),
                                         put_request_.value());
  put_response_.set_success(s.ok());
  float ew = put_request_.ew();

  // std::cout << "ew: " << ew << std::endl;

  if (ew == TTL_EW) {
    // Disabled
    return;
  } else {
    // std::cout << "InvalidateOrUpdate starts!" << std::endl;

    // if (STALENESS_BOUND_IN_MS == 0 || ew == INVALIDATE_EW || ew == UPDATE_EW)
    // For invalidate and update, send it out.
    server_->InvalidateOrUpdate(put_request_.key(), put_request_.value(), ew);
    // else
    // server_->WriteBuffer(put_request_.key(), put_request_.value(), ew);
    // std::cout << "InvalidateOrUpdate finishes!" << std::endl;
  }
  server_->set_count()++;
}

// CallDataGet Implementation
CallDataGet::CallDataGet(DBService::AsyncService *service,
                         ServerCompletionQueue *cq, AsyncServer *server)
    : service_(service),
      cq_(cq),
      server_(server),
      responder_(&ctx_),
      status_(CREATE) {
  Proceed();
}

void CallDataGet::Proceed() {
  if (status_ == CREATE) {
    status_ = PROCESS;
    // Request a new Get RPC
    // std::cout << "CallDataGet start" << std::endl;
    service_->RequestGet(&ctx_, &get_request_, &responder_, cq_, cq_, this);
    // std::cout << "DB after RequestGet" << std::endl;
  } else if (status_ == PROCESS) {
    // Spawn a new CallDataGet instance to serve new clients while we process
    // the current one
    new CallDataGet(service_, cq_, server_);
    // std::cout << "DB Create a new CallDataGet instance" << std::endl;

    // Process the request
    HandleGet();

    status_ = FINISH;
    // std::cout << "DB HandleGet finishes!" << std::endl;
    responder_.Finish(get_response_, Status::OK, this);
    // std::cout << "CallDataGet finish" << std::endl;
    // std::cout << "DB responder_ finishes" << std::endl;
  } else {
    // Clean up and delete this instance
    // std::cout << "DB clean up and delete this instance" << std::endl;
    delete this;
  }
}

void CallDataGet::HandleGet() {
  std::string value;
  // std::cout << "DB Get: " << get_request_.key() << std::endl;
  rocksdb::Status s =
      server_->db()->Get(rocksdb::ReadOptions(), get_request_.key(), &value);

  // std::cout << "DB Get finished: " << get_request_.key() << ", " <<
  // value.size()
  // << std::endl;
  if (s.ok()) {
    get_response_.set_value(value);
    get_response_.set_found(true);
  } else {
    // std::cout << "DB: Get couldn't find the value. " << std::endl;
    get_response_.set_value("");
    get_response_.set_found(false);
  }
  server_->load() += C_M;
  // server_->set_miss(get_request_.key());
  // server_->is_key_invalidated()[get_request_.key()] = false;
  server_->get_count()++;
}

// CallDataDelete Implementation
CallDataDelete::CallDataDelete(DBService::AsyncService *service,
                               ServerCompletionQueue *cq, AsyncServer *server)
    : service_(service),
      cq_(cq),
      server_(server),
      responder_(&ctx_),
      status_(CREATE) {
  Proceed();
}

void CallDataDelete::Proceed() {
  if (status_ == CREATE) {
    status_ = PROCESS;
    // Request a new Delete RPC
    service_->RequestDelete(&ctx_, &delete_request_, &responder_, cq_, cq_,
                            this);
  } else if (status_ == PROCESS) {
    // Spawn a new CallDataDelete instance to serve new clients while we
    // process the current one
    new CallDataDelete(service_, cq_, server_);

    // Process the request
    HandleDelete();

    status_ = FINISH;
    responder_.Finish(delete_response_, Status::OK, this);
  } else {
    // Clean up and delete this instance
    delete this;
  }
}

void CallDataDelete::HandleDelete() {
  rocksdb::Status s =
      server_->db()->Delete(rocksdb::WriteOptions(), delete_request_.key());
  delete_response_.set_success(s.ok());
}

// CallDataGetLoad Implementation
CallDataGetLoad::CallDataGetLoad(DBService::AsyncService *service,
                                 ServerCompletionQueue *cq, AsyncServer *server)
    : service_(service),
      cq_(cq),
      server_(server),
      responder_(&ctx_),
      status_(CREATE) {
  Proceed();
}

void CallDataGetLoad::Proceed() {
  if (status_ == CREATE) {
    status_ = PROCESS;
    // Request a new GetLoad RPC
    service_->RequestGetLoad(&ctx_, &getload_request_, &responder_, cq_, cq_,
                             this);
  } else if (status_ == PROCESS) {
    // Spawn a new CallDataGetLoad instance to serve new clients while we
    // process the current one
    new CallDataGetLoad(service_, cq_, server_);

    // Process the request
    HandleGetLoad();

    status_ = FINISH;
    responder_.Finish(getload_response_, Status::OK, this);
  } else {
    // Clean up and delete this instance
    delete this;
  }
}

void CallDataGetLoad::HandleGetLoad() {
  getload_response_.set_load(server_->load());
  getload_response_.set_success(true);
  // Reset Load.
  server_->load() = 0;
}

// CallDataStartRecord Implementation
CallDataStartRecord::CallDataStartRecord(DBService::AsyncService *service,
                                         ServerCompletionQueue *cq,
                                         AsyncServer *server)
    : service_(service),
      cq_(cq),
      server_(server),
      responder_(&ctx_),
      status_(CREATE) {
  Proceed();
}

void CallDataStartRecord::Proceed() {
  if (status_ == CREATE) {
    status_ = PROCESS;
    // Request a new StartRecord RPC
    service_->RequestStartRecord(&ctx_, &request_, &responder_, cq_, cq_, this);
  } else if (status_ == PROCESS) {
    // Spawn a new CallDataStartRecord instance to serve new clients while
    // we process the current one
    new CallDataStartRecord(service_, cq_, server_);

    // Process the request
    HandleStartRecord();

    status_ = FINISH;
    responder_.Finish(response_, Status::OK, this);
  } else {
    // Clean up and delete this instance
    delete this;
  }
}

void CallDataStartRecord::HandleStartRecord() {
  // Extract log path from request if necessary
  // std::string log_path = "";  // Set log_path from request fields if
  // provided

  std::cout << "Start Recording: " << log_path << std::endl;
  // Call START_COLLECTION with log_path
  START_COLLECTION(log_path, server_, server_->cache_client());

  // Set the response success flag
  response_.set_success(true);
}

// CallDataGetReadCount Implementation
CallDataGetReadCount::CallDataGetReadCount(DBService::AsyncService *service,
                                           ServerCompletionQueue *cq,
                                           AsyncServer *server)
    : service_(service),
      cq_(cq),
      server_(server),
      responder_(&ctx_),
      status_(CREATE) {
  Proceed();
}

void CallDataGetReadCount::Proceed() {
  if (status_ == CREATE) {
    status_ = PROCESS;
    // Request a new GetReadCount RPC
    service_->RequestGetReadCount(&ctx_, &request_, &responder_, cq_, cq_,
                                  this);
  } else if (status_ == PROCESS) {
    // Spawn a new CallDataGetReadCount instance to serve new clients while we
    // process the current one
    new CallDataGetReadCount(service_, cq_, server_);

    // Process the request
    HandleGetReadCount();

    status_ = FINISH;
    responder_.Finish(response_, Status::OK, this);
  } else {
    // Clean up and delete this instance
    delete this;
  }
}

void CallDataGetReadCount::HandleGetReadCount() {
  response_.set_read_count(server_->get_count().load());
  response_.set_success(true);
}

// CallDataGetWriteCount Implementation
CallDataGetWriteCount::CallDataGetWriteCount(DBService::AsyncService *service,
                                             ServerCompletionQueue *cq,
                                             AsyncServer *server)
    : service_(service),
      cq_(cq),
      server_(server),
      responder_(&ctx_),
      status_(CREATE) {
  Proceed();
}

void CallDataGetWriteCount::Proceed() {
  if (status_ == CREATE) {
    status_ = PROCESS;
    // Request a new GetWriteCount RPC
    service_->RequestGetWriteCount(&ctx_, &request_, &responder_, cq_, cq_,
                                   this);
  } else if (status_ == PROCESS) {
    // Spawn a new CallDataGetWriteCount instance to serve new clients while
    // we process the current one
    new CallDataGetWriteCount(service_, cq_, server_);

    // Process the request
    HandleGetWriteCount();

    status_ = FINISH;
    responder_.Finish(response_, Status::OK, this);
  } else {
    // Clean up and delete this instance
    delete this;
  }
}

void CallDataGetWriteCount::HandleGetWriteCount() {
  response_.set_write_count(server_->set_count().load());
  response_.set_success(true);
}

int main(int argc, char **argv) {
  // Default values
  std::string port = "50051";
  std::string rocksdb_path = "test.db";
  CacheClient cache_client(grpc::CreateChannel(
      "10.128.0.39:50051", grpc::InsecureChannelCredentials()));

  // Check the number of arguments
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <port> <rocksdb_path> <log_path>"
              << std::endl;
    return 1;  // Return with error code
  }

  // Read arguments
  port = argv[1];
  rocksdb_path = argv[2];
  log_path = argv[3];

  // Construct the server address
  std::string server_address = "0.0.0.0:" + port;

  AsyncServer server(server_address, rocksdb_path, &cache_client);

  // START_COLLECTION(log_path);
  server.Run();
  END_COLLECTION();

  return 0;
}
