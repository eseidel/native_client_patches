// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include "ppapi/c/ppp.h"

/*
 * Returns PP_OK on success, any other value on failure. Failure indicates to
 * the browser that this plugin can not be used. In this case, the plugin will
 * be unloaded and ShutdownModule will NOT be called.
 */
PP_EXPORT int32_t PPP_InitializeModule(PP_Module module,
                                       PPB_GetInterface get_browser_interface);


void PPAPI_GetInterface()

const void* PPAPI_GetBrowserInterface(const char* interface_name)
{
    return ppapi_call(NULL);
}

void InitializeModule()
{
    if (PPP_InitializeModule(1, PPAPI_GetBrowserInterface) != PP_OK)
        return;
    PPP_GetInterface
}

struct PPAPICall {
    int call_id;
    void* args;
};

struct InitializeModuleArgs {
    PP_Module module;
}

void PPAPI_HandleBrowserCall(PPAPICall* call)
{
    
}

int main(int argc, char** argv) {
    printf("Hello PPAPI!\n");
    while (1)
        PPAPICall call;
        bzero(&call, sizeof(call));
        if (!ppapi_waitnextevent((void*)&call)))
            return 0;
    }
}
