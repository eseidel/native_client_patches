// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"

LaunchNaClProcessFunc launch_nacl_process = NULL;

namespace nacl {
  bool SelLdrLauncher::StartFromBrowser(const char* url, int socket_count,
                                        Handle* result_sockets) {
    // send a synchronous message to the browser process
    Handle nacl_proc_handle;
    int nacl_proc_id;
    if (!launch_nacl_process ||
        !launch_nacl_process(url,
                             socket_count,
                             result_sockets,
                             &nacl_proc_handle,
                             &nacl_proc_id)) {
      return false;
    }

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
    NaClHandlePassBrowserRememberHandle(nacl_proc_id, nacl_proc_handle);
#endif

    CloseHandlesAfterLaunch();
    // TODO(gregoryd): the handle is currently returned on Windows only.
    child_process_ = nacl_proc_handle;
    return true;
  }
}
