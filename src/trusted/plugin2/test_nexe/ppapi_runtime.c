// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <sys/nacl_syscalls.h>

#include "ppapi/c/ppp.h"

#include "ppapi_call_from_browser.h"

int main(int argc, char** argv) {
    printf("Hello PPAPI!\n");
    CallFromBrowser call;
    // We only initialize call at the start.
    bzero(&call, sizeof(call));
    while (1) {
        // Ask the browser what we should do next.
        // This also passes the return value from the previous browser->NaCl call.
        // If waitnextevent returns non-zero, we should exit.
        if (ppapi_waitnextevent((void*)&call))
            break;
        HandleCallFromBrowser(&call);
    }
    printf("ppapi_waitnextevent returned non-zero, exiting.\n");
    return 0;
}
