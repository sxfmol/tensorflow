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

#ifndef TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_MASTER_INTERFACE_H_
#define TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_MASTER_INTERFACE_H_

#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/protobuf/master.pb.h"

namespace tensorflow {

// Pure virtual interface for communicating with the TensorFlow Master service.
//
// This interface is intended to support in-process master
// implementations that do not require an RPC roundtrip.
class MasterInterface {
 public:
  virtual ~MasterInterface() {}
  virtual Status CreateSession(const CreateSessionRequest* request,
                               CreateSessionResponse* response) = 0;

  virtual Status ExtendSession(const ExtendSessionRequest* request,
                               ExtendSessionResponse* response) = 0;

  virtual Status RunStep(const RunStepRequest* request,
                         RunStepResponse* response) = 0;

  virtual Status CloseSession(const CloseSessionRequest* request,
                              CloseSessionResponse* response) = 0;

  virtual Status ListDevices(const ListDevicesRequest* request,
                             ListDevicesResponse* response) = 0;

  virtual Status Reset(const ResetRequest* request,
                       ResetResponse* response) = 0;
};

}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_DISTRIBUTED_RUNTIME_MASTER_INTERFACE_H_
