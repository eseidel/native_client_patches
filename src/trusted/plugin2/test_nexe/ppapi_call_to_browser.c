#include "ppapi_call_to_browser.h"

#include <string.h>

#include <sys/nacl_syscalls.h>

// We supply our own get_browser_interface function
// which instead of being a direct function pointer uses ppapi_call
const void* GetBrowserInterface(const char* interface_name)
{
    CallToBrowser call;
    call.call_id = PPAPICallGetBrowserInterface;
    GetBrowserInterfaceArgs args;
    bzero(&args, sizeof(args));
    args.interface_name = interface_name;
    call.args = &args;
    if (!ppapi_call(&call))
        return 0;
    return args.return_value;
}