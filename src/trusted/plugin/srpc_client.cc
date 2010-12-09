/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <string.h>

#include <map>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/connected_socket.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc_client.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace {

// TODO(sehr): again, not reentrant.  See bug 605.
PLUGIN_JMPBUF srpc_env;

void SignalHandler(int value) {
  PLUGIN_PRINTF(("SrpcClient::SignalHandler()\n"));
  PLUGIN_LONGJMP(srpc_env, value);
}

}  // namespace

namespace plugin {

SrpcClient::SrpcClient()
    : srpc_channel_initialised_(false),
      browser_interface_(NULL) {
  PLUGIN_PRINTF(("SrpcClient::SrpcClient (this=%p)\n",
                 static_cast<void*>(this)));
}

bool SrpcClient::Init(BrowserInterface* browser_interface,
                      ConnectedSocket* socket) {
  PLUGIN_PRINTF(("SrpcClient::Init (this=%p, browser_interface=%p, socket=%p)"
                 "\n", static_cast<void*>(this),
                 static_cast<void*>(browser_interface),
                 static_cast<void*>(socket)));
  // Open the channel to pass RPC information back and forth
  if (!NaClSrpcClientCtor(&srpc_channel_, socket->wrapper()->desc())) {
    return false;
  }
  srpc_channel_initialised_ = true;
  browser_interface_ = browser_interface;
  PLUGIN_PRINTF(("SrpcClient::Init (Ctor worked)\n"));
  // Record the method names in a convenient way for later dispatches.
  GetMethods();
  PLUGIN_PRINTF(("SrpcClient::Init (GetMethods worked)\n"));
  return true;
}

SrpcClient::~SrpcClient() {
  PLUGIN_PRINTF(("SrpcClient::~SrpcClient (this=%p, has_srpc_channel=%d)\n",
                 static_cast<void*>(this), srpc_channel_initialised_));
  // And delete the connection.
  if (srpc_channel_initialised_) {
    PLUGIN_PRINTF(("SrpcClient::~SrpcClient (destroying srpc_channel)\n"));
    NaClSrpcDtor(&srpc_channel_);
  }
  for (Methods::iterator iter = methods_.begin();
       iter != methods_.end();
       ++iter) {
    delete iter->second;
  }
  PLUGIN_PRINTF(("SrpcClient::~SrpcClient (return)\n"));
}

void SrpcClient::StartJSObjectProxy(Plugin* plugin) {
  // Start up NPAPI/PPAPI interaction if the plugin determines that the
  // requisite methods are exported.
  plugin->StartProxiedExecution(&srpc_channel_);
}

void SrpcClient::GetMethods() {
  PLUGIN_PRINTF(("SrpcClient::GetMethods (this=%p)\n",
                 static_cast<void*>(this)));
  if (NULL == srpc_channel_.client) {
    return;
  }
  uint32_t method_count = NaClSrpcServiceMethodCount(srpc_channel_.client);
  // Intern the methods into a mapping from NPIdentifiers to MethodInfo.
  for (uint32_t i = 0; i < method_count; ++i) {
    int retval;
    const char* name;
    const char* input_types;
    const char* output_types;

    retval = NaClSrpcServiceMethodNameAndTypes(srpc_channel_.client,
                                               i,
                                               &name,
                                               &input_types,
                                               &output_types);
    if (!IsValidIdentifierString(name, NULL)) {
      // If name is not an ECMAScript identifier, do not enter it into the
      // methods_ table.
      continue;
    }
    uintptr_t ident = browser_interface_->StringToIdentifier(name);
    MethodInfo* method_info = new(std::nothrow) MethodInfo(NULL,
                                                           name,
                                                           input_types,
                                                           output_types,
                                                           i);
    if (NULL == method_info) {
      return;
    }
    // Install in the map only if successfully read.
    methods_[ident] = method_info;
  }
}

bool SrpcClient::HasMethod(uintptr_t method_id) {
  MethodInfo* method_info = methods_[method_id];
  PLUGIN_PRINTF(("SrpcClient::HasMethod (this=%p, return %p)\n",
                 static_cast<void*>(this), static_cast<void*>(method_info)));
  return NULL != method_info;
}

bool SrpcClient::InitParams(uintptr_t method_id, SrpcParams* params) {
  MethodInfo* method_info = methods_[method_id];
  if (method_info) {
    return params->Init(method_info->ins(), method_info->outs());
  }
  return false;
}

bool SrpcClient::Invoke(uintptr_t method_id,
                        SrpcParams* params) {
  // It would be better if we could set the exception on each detailed failure
  // case.  However, there are calls to Invoke from within the plugin itself,
  // and these could leave residual exceptions pending.  This seems to be
  // happening specifically with hard_shutdowns.
  PLUGIN_PRINTF(("SrpcClient::Invoke (this=%p, method_name='%s', params=%p)\n",
                 static_cast<void*>(this),
                 browser_interface_->IdentifierToString(method_id).c_str(),
                 static_cast<void*>(params)));

  // Ensure Invoke was called with an identifier that had a binding.
  if (NULL == methods_[method_id]) {
    PLUGIN_PRINTF(("SrpcClient::Invoke (ident not in methods_)\n"));
    return false;
  }

  // Catch signals from SRPC/IMC/etc.
  ScopedCatchSignals sigcatcher(
      (ScopedCatchSignals::SigHandlerType) SignalHandler);

  PLUGIN_PRINTF(("SrpcClient::Invoke (sending the rpc)\n"));
  // Call the method
  NaClSrpcError err = NaClSrpcInvokeV(&srpc_channel_,
                                      methods_[method_id]->index(),
                                      params->ins(),
                                      params->outs());
  PLUGIN_PRINTF(("SrpcClient::Invoke (response=%d)\n", err));
  if (NACL_SRPC_RESULT_OK != err) {
    PLUGIN_PRINTF(("SrpcClient::Invoke (err='%s', return 0)\n",
                   NaClSrpcErrorString(err)));
    return false;
  }

  PLUGIN_PRINTF(("SrpcClient::Invoke (return 1)\n"));
  return true;
}

}  // namespace plugin
