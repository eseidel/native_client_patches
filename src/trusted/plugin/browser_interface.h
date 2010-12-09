/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_

#include <stdio.h>
#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/api_defines.h"

namespace nacl {

class NPModule;

}  // namespace

namespace plugin {

class ScriptableHandle;
class PortableHandle;

// BrowserInterface represents the interface to the browser from
// the plugin, independent of whether it is the ActiveX or NPAPI instance.
// I.e., when the plugin needs to request an alert, it uses these interfaces.
class BrowserInterface {
 public:
  virtual ~BrowserInterface() { }

  // Functions for communication with the browser.

  // Convert a string to an identifier.
  virtual uintptr_t StringToIdentifier(const nacl::string& str) = 0;
  // Convert an identifier to a string.
  virtual nacl::string IdentifierToString(uintptr_t ident) = 0;

  // Pops up an alert box.  Returns false if that failed for any reason.
  virtual bool Alert(InstanceIdentifier instance_id,
                     const nacl::string& text) = 0;

  // Evaluate a JavaScript string in the browser.
  virtual bool EvalString(InstanceIdentifier instance_id,
                          const nacl::string& str) = 0;

  // Gets the full URL of the current page.
  virtual bool GetFullURL(InstanceIdentifier instance_id,
                          nacl::string* full_url) = 0;

  // Write to the JavaScript console. Currently works in Chrome only, generates
  // an alert in other browsers.
  virtual bool AddToConsole(InstanceIdentifier instance_id,
                            const nacl::string& text) = 0;

  // Gets the origin of the current page.  Origin is scheme://domain.
  bool GetOrigin(InstanceIdentifier instance_id,
                 nacl::string* origin);

  // Creates a browser scriptable handle for a given portable handle.
  // If handle is NULL, returns NULL.
  virtual ScriptableHandle* NewScriptableHandle(PortableHandle* handle) = 0;

  // Returns true iff the first |size| bytes of |e_ident_bytes| appear to be
  // a valid ELF file; returns an informative error message otherwise.
  // The check for valid ELF executable is only done looking at the e_ident
  // bytes.  Fuller checking is done by the service_runtime.
  static bool MightBeElfExecutable(const char* e_ident_bytes,
                                   size_t size,
                                   nacl::string* error);
  // Wrappers of the above function that load the file contents given
  // a filename, a file descriptor, or a file stream.
  static bool MightBeElfExecutable(const nacl::string& filename,
                                   nacl::string* error);
  static bool MightBeElfExecutable(int posix_file_desc,
                                   nacl::string* error);
  static bool MightBeElfExecutable(FILE* file_stream,  // Will close the stream.
                                   nacl::string* error);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_BROWSER_INTERFACE_H_
