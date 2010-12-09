// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#ifndef GEN_PPAPI_PROXY_PPP_RPC_H_
#define GEN_PPAPI_PROXY_PPP_RPC_H_
#ifdef __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif  // __native_client__

class ObjectStubRpcServer {
 public:
  static void HasProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static void HasMethod(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* success,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static void GetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* value_bytes, char* value,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static void GetAllPropertyNames(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      int32_t* property_count,
      nacl_abi_size_t* properties_bytes, char* properties,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static void SetProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t value_bytes, char* value,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static void RemoveProperty(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static void Call(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      nacl_abi_size_t name_bytes, char* name,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static void Construct(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability,
      int32_t argc,
      nacl_abi_size_t argv_bytes, char* argv,
      nacl_abi_size_t exception_in_bytes, char* exception_in,
      nacl_abi_size_t* ret_bytes, char* ret,
      nacl_abi_size_t* exception_bytes, char* exception
  );
  static void Deallocate(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      nacl_abi_size_t capability_bytes, char* capability
  );

 private:
  ObjectStubRpcServer();
  ObjectStubRpcServer(const ObjectStubRpcServer&);
  void operator=(const ObjectStubRpcServer);

};  // class ObjectStubRpcServer

class CompletionCallbackRpcServer {
 public:
  static void InvokeCompletionCallback(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t callback_id,
      int32_t result
  );

 private:
  CompletionCallbackRpcServer();
  CompletionCallbackRpcServer(const CompletionCallbackRpcServer&);
  void operator=(const CompletionCallbackRpcServer);

};  // class CompletionCallbackRpcServer

class PppRpcServer {
 public:
  static void PPP_InitializeModule(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int32_t pid,
      int64_t module,
      NaClSrpcImcDescType upcall_channel_desc,
      char* service_description,
      int32_t* nacl_pid,
      int32_t* success
  );
  static void PPP_ShutdownModule(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done
  );
  static void PPP_GetInterface(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      char* interface_name,
      int32_t* exports_interface_name
  );

 private:
  PppRpcServer();
  PppRpcServer(const PppRpcServer&);
  void operator=(const PppRpcServer);

};  // class PppRpcServer

class PppInstanceRpcServer {
 public:
  static void PPP_Instance_DidCreate(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t instance,
      int32_t argc,
      nacl_abi_size_t argn_bytes, char* argn,
      nacl_abi_size_t argv_bytes, char* argv,
      int32_t* success
  );
  static void PPP_Instance_DidDestroy(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t instance
  );
  static void PPP_Instance_DidChangeView(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t instance,
      nacl_abi_size_t position_bytes, int32_t* position,
      nacl_abi_size_t clip_bytes, int32_t* clip
  );
  static void PPP_Instance_DidChangeFocus(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t instance,
      bool has_focus
  );
  static void PPP_Instance_HandleInputEvent(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t instance,
      nacl_abi_size_t event_data_bytes, char* event_data,
      int32_t* success
  );
  static void PPP_Instance_HandleDocumentLoad(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t instance,
      int64_t url_loader,
      int32_t* success
  );
  static void PPP_Instance_GetInstanceObject(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      int64_t instance,
      nacl_abi_size_t* capability_bytes, char* capability
  );

 private:
  PppInstanceRpcServer();
  PppInstanceRpcServer(const PppInstanceRpcServer&);
  void operator=(const PppInstanceRpcServer);

};  // class PppInstanceRpcServer

class PppRpcs {
 public:
  static NACL_SRPC_METHOD_ARRAY(srpc_methods);
};  // class PppRpcs

#endif  // GEN_PPAPI_PROXY_PPP_RPC_H_

