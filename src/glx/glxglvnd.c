#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>

#include <X11/Xlib.h>
#include <X11/Xlibint.h>

#include "glvnd/libglxabi.h"

#include "glx_error.h"
#include "glxglvnd.h"

#include <assert.h>

_X_EXPORT __GLX_MAIN_PROTO(version, exports, vendorName);

static Bool initDone = False;

static Bool __glXGLVNDSupportsScreen(Display *dpy, int screen)
{
    return True;
}

static void *__glXGLVNDGetProcAddress(const GLubyte *procName)
{
    return glXGetProcAddressARB(procName);
}

static int FindGLXFunction(const GLubyte *name)
{
    int i;

    for (i = 0; i < DI_FUNCTION_COUNT; i++) {
        if (strcmp((const char *) name, __glXDispatchTableStrings[i]) == 0)
            return i;
    }
    return -1;
}

static void *__glXGLVNDGetDispatchAddress(const GLubyte *procName)
{
    int internalIndex = FindGLXFunction(procName);

    if (internalIndex >= 0) {
        return __glXDispatchFunctions[internalIndex];

    return NULL;
}

static void __glXGLVNDSetDispatchIndex(const GLubyte *procName, int index)
{
    int internalIndex = FindGLXFunction(procName);

    if (internalIndex >= 0)
        __glXDispatchTableIndices[internalIndex] = index;
}

static __GLXapiImports glvndImports = {
    __glXGLVNDSupportsScreen, // checkSupportsScreen
    __glXGLVNDGetProcAddress, // getProcAddress
    __glXGLVNDGetDispatchAddress, // getDispatchAddress
    __glXGLVNDSetDispatchIndex, // setDispatchIndex
    NULL, // notifyError
    NULL // patchCallbacks
};

__GLX_MAIN_PROTO(version, exports, vendorName)
{
    if (version != GLX_VENDOR_ABI_VERSION)
        return NULL;

    if (!initDone) {
        initDone = True;
        __glXGLVNDInitDispatchFunctions();
        memcpy(&__glXGLVNDAPIExports, exports, sizeof(*exports));
    }

    return &glvndImports;
}
