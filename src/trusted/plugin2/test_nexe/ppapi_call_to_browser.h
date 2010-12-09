/* This file needs to be shared between the trusted and untrusted code.
 * This should also be autogenated.
 */

typedef enum {
    PPAPICallGetBrowserInterface = 1, // Not a part of PPAPI, rather a function pointer ppapi normaly passes.
} CallToBrowserId;

typedef struct {
    CallToBrowserId call_id;
    void* args;
} CallToBrowser;

typedef struct {
    void* return_value;
    const char* interface_name;
} GetBrowserInterfaceArgs;

// Internal
const void* GetBrowserInterface(const char* interface_name);
