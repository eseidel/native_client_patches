// File for handling calls from the trusted code to the untrusted code.

#include <stdio.h>

#include "ppapi_call_from_browser.h"
#include "ppapi_call_to_browser.h"

void HandleCallFromBrowser(CallFromBrowser* call)
{
    printf("Called\n");
    switch(call->call_id) {
    case PPAPICallInitializeModule: {
        InitializeModuleArgs* args = call->args;
        args->return_value = PPP_InitializeModule(args->module, GetBrowserInterface);
        break;
    }
    case PPAPICallShutdownModule:
        PPP_ShutdownModule();
        break;
    case PPAPICallGetInterface: {
        GetInterfaceArgs* args = call->args;
        args->return_value = PPP_GetInterface(args->interface_name);
        break;
    }
    }
}
