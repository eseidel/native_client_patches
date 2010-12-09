#include "ppapi/c/ppp.h"

typedef enum {
    PPAPICallInitializeModule = 0,
    PPAPICallShutdownModule = 1,
    PPAPICallGetInterface = 2,
} CallFromBrowserId;

typedef struct {
    CallFromBrowserId call_id;
    void* args;
} CallFromBrowser;

typedef struct {
    int32_t return_value;
    PP_Module module;
} InitializeModuleArgs;

typedef struct {
    const void* return_value;
    char* interface_name;
} GetInterfaceArgs;

void HandleCallFromBrowser(CallFromBrowser*);
