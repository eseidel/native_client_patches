// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "untrusted/srpcgen/ppp_rpc.h"
#ifdef __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif  // __native_client__

namespace {

static void HasPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::HasProperty(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void HasMethodDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::HasMethod(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void GetPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::GetProperty(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void GetAllPropertyNamesDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::GetAllPropertyNames(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.count), outputs[1]->arrays.carr,
      &(outputs[2]->u.count), outputs[2]->arrays.carr
  );
}

static void SetPropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::SetProperty(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void RemovePropertyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::RemoveProperty(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

static void CallDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::Call(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      inputs[2]->u.ival,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      inputs[4]->u.count, inputs[4]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void ConstructDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::Construct(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      &(outputs[0]->u.count), outputs[0]->arrays.carr,
      &(outputs[1]->u.count), outputs[1]->arrays.carr
  );
}

static void DeallocateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
  ObjectStubRpcServer::Deallocate(
      rpc,
      done,
      inputs[0]->u.count, inputs[0]->arrays.carr
  );
}

static void InvokeCompletionCallbackDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
  CompletionCallbackRpcServer::InvokeCompletionCallback(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.ival
  );
}

static void PPP_InitializeModuleDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  PppRpcServer::PPP_InitializeModule(
      rpc,
      done,
      inputs[0]->u.ival,
      inputs[1]->u.lval,
      inputs[2]->u.hval,
      inputs[3]->arrays.str,
      &(outputs[0]->u.ival),
      &(outputs[1]->u.ival)
  );
}

static void PPP_ShutdownModuleDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(inputs);
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
  PppRpcServer::PPP_ShutdownModule(
      rpc,
      done
  );
}

static void PPP_GetInterfaceDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  PppRpcServer::PPP_GetInterface(
      rpc,
      done,
      inputs[0]->arrays.str,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Instance_DidCreateDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  PppInstanceRpcServer::PPP_Instance_DidCreate(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.ival,
      inputs[2]->u.count, inputs[2]->arrays.carr,
      inputs[3]->u.count, inputs[3]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Instance_DidDestroyDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
  PppInstanceRpcServer::PPP_Instance_DidDestroy(
      rpc,
      done,
      inputs[0]->u.lval
  );
}

static void PPP_Instance_DidChangeViewDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
  PppInstanceRpcServer::PPP_Instance_DidChangeView(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.count, inputs[1]->arrays.iarr,
      inputs[2]->u.count, inputs[2]->arrays.iarr
  );
}

static void PPP_Instance_DidChangeFocusDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);
  PppInstanceRpcServer::PPP_Instance_DidChangeFocus(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.bval
  );
}

static void PPP_Instance_HandleInputEventDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  PppInstanceRpcServer::PPP_Instance_HandleInputEvent(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.count, inputs[1]->arrays.carr,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Instance_HandleDocumentLoadDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  PppInstanceRpcServer::PPP_Instance_HandleDocumentLoad(
      rpc,
      done,
      inputs[0]->u.lval,
      inputs[1]->u.lval,
      &(outputs[0]->u.ival)
  );
}

static void PPP_Instance_GetInstanceObjectDispatcher(
    NaClSrpcRpc* rpc,
    NaClSrpcArg** inputs,
    NaClSrpcArg** outputs,
    NaClSrpcClosure* done
) {
  UNREFERENCED_PARAMETER(done);
  PppInstanceRpcServer::PPP_Instance_GetInstanceObject(
      rpc,
      done,
      inputs[0]->u.lval,
      &(outputs[0]->u.count), outputs[0]->arrays.carr
  );
}

}  // namespace

NACL_SRPC_METHOD_ARRAY(PppRpcs::srpc_methods) = {
  { "HasProperty:CCC:iC", HasPropertyDispatcher },
  { "HasMethod:CCC:iC", HasMethodDispatcher },
  { "GetProperty:CCC:CC", GetPropertyDispatcher },
  { "GetAllPropertyNames:CC:iCC", GetAllPropertyNamesDispatcher },
  { "SetProperty:CCCC:C", SetPropertyDispatcher },
  { "RemoveProperty:CCC:C", RemovePropertyDispatcher },
  { "Call:CCiCC:CC", CallDispatcher },
  { "Construct:CiCC:CC", ConstructDispatcher },
  { "Deallocate:C:", DeallocateDispatcher },
  { "InvokeCompletionCallback:ii:", InvokeCompletionCallbackDispatcher },
  { "PPP_InitializeModule:ilhs:ii", PPP_InitializeModuleDispatcher },
  { "PPP_ShutdownModule::", PPP_ShutdownModuleDispatcher },
  { "PPP_GetInterface:s:i", PPP_GetInterfaceDispatcher },
  { "PPP_Instance_DidCreate:liCC:i", PPP_Instance_DidCreateDispatcher },
  { "PPP_Instance_DidDestroy:l:", PPP_Instance_DidDestroyDispatcher },
  { "PPP_Instance_DidChangeView:lII:", PPP_Instance_DidChangeViewDispatcher },
  { "PPP_Instance_DidChangeFocus:lb:", PPP_Instance_DidChangeFocusDispatcher },
  { "PPP_Instance_HandleInputEvent:lC:i", PPP_Instance_HandleInputEventDispatcher },
  { "PPP_Instance_HandleDocumentLoad:ll:i", PPP_Instance_HandleDocumentLoadDispatcher },
  { "PPP_Instance_GetInstanceObject:l:C", PPP_Instance_GetInstanceObjectDispatcher },
  { NULL, NULL }
};  // NACL_SRPC_METHOD_ARRAY

