/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// A class containing information regarding a socket connection to a
// service runtime instance.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace nacl {
class DescWrapper;
struct SelLdrLauncher;
}  // namespace


namespace plugin {

class BrowserInterface;
class ConnectedSocket;
class Plugin;
class SocketAddress;
class SrtSocket;
class ScriptableHandle;

//  ServiceRuntime abstracts a NativeClient sel_ldr instance.
class ServiceRuntime {
 public:
  // TODO(sehr): This class should also implement factory methods, using the
  // current contents of the Start methods below.
  explicit ServiceRuntime(Plugin* plugin);
  // The destructor terminates the sel_ldr process.
  ~ServiceRuntime();

  // Support for spawning sel_ldr instance and establishing a ConnectedSocket
  // to it. The sel_ldr process can be started directly using a command line
  // or by sending a message to the browser process (in Chrome only).
  // The nexe can be passed:
  // 1) via a local file (typically from browser cache, outside of a sandbox)
  //    as command-line argument
  // 2) as a file descriptor over RPC after sel_ldr launched
  // 3) as shared memory descriptor over RPC after sel_ldr launched
  // These 3 nexe options are mutually exclusive and map to the following usage:
  // 1) StartFromCommandLine(file_path, NULL)
  // 2) StartFromCommandLine(NACL_NO_FILE_PATH, file_desc)
  //    StartFromBrowser(url, file_desc)
  // 3) StartFromBrowser(url, shm_desc)
  bool StartFromCommandLine(nacl::string nacl_file,
                            nacl::DescWrapper* nacl_file_desc);
  bool StartFromBrowser(nacl::string nacl_url, nacl::DescWrapper* nacl_desc);

  bool Kill();
  bool Log(int severity, nacl::string msg);
  ScriptableHandle* default_socket_address() const;
  ScriptableHandle* GetSocketAddress(Plugin* plugin, nacl::Handle channel);
  Plugin* plugin() const { return plugin_; }
  void Shutdown();

  nacl::DescWrapper* async_receive_desc() { return async_receive_desc_; }
  nacl::DescWrapper* async_send_desc() { return async_send_desc_; }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ServiceRuntime);
  bool InitCommunication(nacl::Handle bootstrap_socket, nacl::DescWrapper* shm);

  // Logs the error, deletes subprocess_ and returns false, so the call
  // to this function can be used as a return value.
  bool Failure(const nacl::string& error);

  ScriptableHandle* default_socket_address_;  // creates, but does not own
  Plugin* plugin_;
  BrowserInterface* browser_interface_;
  SrtSocket* runtime_channel_;
  nacl::SelLdrLauncher* subprocess_;

  // We need two IMC sockets rather than one because IMC sockets are
  // not full-duplex on Windows.
  // See http://code.google.com/p/nativeclient/issues/detail?id=690.
  // TODO(mseaborn): We should not have to work around this.
  nacl::DescWrapper* async_receive_desc_;
  nacl::DescWrapper* async_send_desc_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SERVICE_RUNTIME_H_
