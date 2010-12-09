// File for handling calls from the trusted code to the untrusted code.

#include <stdio.h>

#include "ppapi_call_from_browser.h"
#include "ppapi_call_to_browser.h"

int HandleCallFromBrowser(CallFromBrowser* call)
{
    printf("HandleCallFromBrowser\n");
    switch(call->call_id) {
    case PPAPICallInitializeModule: {
        printf("PPAPICallInitializeModule\n");
        InitializeModuleArgs* args = call->args;
        args->return_value = PPP_InitializeModule(args->module, GetBrowserInterface);
        return 0;
    }
    case PPAPICallShutdownModule:
        printf("PPAPICallShutdownModule\n");
        PPP_ShutdownModule();
        return 0;
    case PPAPICallGetInterface: {
        printf("PPAPICallGetInterface\n");
        GetInterfaceArgs* args = call->args;
        args->return_value = PPP_GetInterface(args->interface_name);
        return 0;
    }
    }
    printf("Invalid call_id: %i\n", call->call_id);
    return 1;
}
