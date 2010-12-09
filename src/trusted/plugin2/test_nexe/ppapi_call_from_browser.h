#include "ppapi/c/ppp.h"

typedef enum {
    // 0 is invalid.
    PPAPICallInitializeModule = 1,
    PPAPICallShutdownModule = 2,
    PPAPICallGetInterface = 3,
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

int HandleCallFromBrowser(CallFromBrowser*);
