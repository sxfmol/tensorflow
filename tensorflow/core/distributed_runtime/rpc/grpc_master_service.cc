/* Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// GrpcMasterService implements the RPC service MasterSerivce.
//
// A GrpcMasterService maintains the state of live graph computation
// sessions, each session orchestrates both local and remote devices
// to carry out the graph computation.
//
// A GrpcMasterService knows ahead of time local devices available as
// client devices.
//
// A GrpcMasterService discovers remote devices in the background and
// keeps track of statistics of those remote devices.
//
// Each session analyses the graph, places nodes across available
// devices, and ultimately drives the graph computation by initiating
// RunGraph on workers.
#include "tensorflow/core/distributed_runtime/rpc/grpc_master_service.h"

#include "external/grpc/include/grpc++/server_builder.h"

#include "tensorflow/core/distributed_runtime/master.h"
#include "tensorflow/core/distributed_runtime/rpc/async_service_interface.h"
#include "tensorflow/core/distributed_runtime/rpc/grpc_call.h"
#include "tensorflow/core/distributed_runtime/rpc/grpc_util.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/protobuf/master.pb.h"
#include "tensorflow/core/protobuf/master_service.grpc.pb.h"

namespace tensorflow {

class GrpcMasterService : public AsyncServiceInterface {
 public:
  GrpcMasterService(MasterEnv* env, ::grpc::ServerBuilder* builder)
      : master_impl_(new Master(env, 0.0)) {
    builder->RegisterService(&master_service_);
    cq_ = builder->AddCompletionQueue().release();
  }

  ~GrpcMasterService() {
    delete cq_;
    delete master_impl_;
  }

// This macro creates a new request for the given RPC method name
// (e.g., `ENQUEUE_REQUEST(RunStep);`), and enqueues it on
// `this->cq_`.
//
// This macro is invoked one or more times for each RPC method to
// ensure that there are sufficient completion queue entries to
// handle incoming requests without blocking.
//
// The implementation of the request handler for each RPC method
// must ensure that it calls ENQUEUE_REQUEST() for that RPC method,
// to keep accepting new requests.
#define ENQUEUE_REQUEST(method)                                             \
  do {                                                                      \
    Call<GrpcMasterService, grpc::MasterService::AsyncService,              \
         method##Request, method##Response>::                               \
        EnqueueRequest(&master_service_, cq_,                               \
                       &grpc::MasterService::AsyncService::Request##method, \
                       &GrpcMasterService::method##Handler);                \
  } while (0)

  void HandleRPCsLoop() {
    ENQUEUE_REQUEST(CreateSession);
    ENQUEUE_REQUEST(ExtendSession);
    for (int i = 0; i < 100; ++i) {
      ENQUEUE_REQUEST(RunStep);
    }
    ENQUEUE_REQUEST(CloseSession);
    ENQUEUE_REQUEST(ListDevices);
    ENQUEUE_REQUEST(Reset);

    void* tag;
    bool ok;
    while (cq_->Next(&tag, &ok)) {
      CHECK(ok);
      UntypedCall<GrpcMasterService>::Tag* callback_tag =
          static_cast<UntypedCall<GrpcMasterService>::Tag*>(tag);
      callback_tag->OnCompleted(this, ok);
      delete callback_tag;
    }
  }

 private:
  Master* master_impl_;                // Owned.
  ::grpc::ServerCompletionQueue* cq_;  // Owned.
  grpc::MasterService::AsyncService master_service_;

  template <class RequestMessage, class ResponseMessage>
  using MasterCall = Call<GrpcMasterService, grpc::MasterService::AsyncService,
                          RequestMessage, ResponseMessage>;

  // RPC handler for creating a session.
  void CreateSessionHandler(
      MasterCall<CreateSessionRequest, CreateSessionResponse>* call) {
    master_impl_->CreateSession(&call->request, &call->response,
                                [call](const Status& status) {
                                  call->SendResponse(ToGrpcStatus(status));
                                });
    ENQUEUE_REQUEST(CreateSession);
  }

  // RPC handler for extending a session.
  void ExtendSessionHandler(
      MasterCall<ExtendSessionRequest, ExtendSessionResponse>* call) {
    master_impl_->ExtendSession(&call->request, &call->response,
                                [call](const Status& status) {
                                  call->SendResponse(ToGrpcStatus(status));
                                });
    ENQUEUE_REQUEST(ExtendSession);
  }

  // RPC handler for running one step in a session.
  void RunStepHandler(MasterCall<RunStepRequest, RunStepResponse>* call) {
    CallOptions* call_opts = new CallOptions;
    call->SetCancelCallback([call_opts]() { call_opts->StartCancel(); });
    master_impl_->RunStep(call_opts, &call->request, &call->response,
                          [call, call_opts](const Status& status) {
                            call->ClearCancelCallback();
                            delete call_opts;
                            call->SendResponse(ToGrpcStatus(status));
                          });
    ENQUEUE_REQUEST(RunStep);
  }

  // RPC handler for deleting a session.
  void CloseSessionHandler(
      MasterCall<CloseSessionRequest, CloseSessionResponse>* call) {
    master_impl_->CloseSession(&call->request, &call->response,
                               [call](const Status& status) {
                                 call->SendResponse(ToGrpcStatus(status));
                               });
    ENQUEUE_REQUEST(CloseSession);
  }

  // RPC handler for listing devices.
  void ListDevicesHandler(
      MasterCall<ListDevicesRequest, ListDevicesResponse>* call) {
    master_impl_->ListDevices(&call->request, &call->response,
                              [call](const Status& status) {
                                call->SendResponse(ToGrpcStatus(status));
                              });
    ENQUEUE_REQUEST(ListDevices);
  }

  // RPC handler for resetting all sessions.
  void ResetHandler(MasterCall<ResetRequest, ResetResponse>* call) {
    master_impl_->Reset(&call->request, &call->response,
                        [call](const Status& status) {
                          call->SendResponse(ToGrpcStatus(status));
                        });
    ENQUEUE_REQUEST(Reset);
  }
#undef ENQUEUE_REQUEST

  TF_DISALLOW_COPY_AND_ASSIGN(GrpcMasterService);
};

AsyncServiceInterface* NewGrpcMasterService(MasterEnv* env,
                                            ::grpc::ServerBuilder* builder) {
  CHECK(!env->local_devices.empty());
  return new GrpcMasterService(env, builder);
}

}  // end namespace tensorflow
