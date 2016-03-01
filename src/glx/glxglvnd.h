#ifndef _glx_lib_glvnd_h_
#define _glx_lib_glvnd_h_

#if defined(__cplusplus)
extern "C" {
#endif

#include "glvnd/libglxabi.h"

extern __GLXapiExports __glXGLVNDAPIExports;

extern const int DI_FUNCTION_COUNT;

extern void *__glXDispatchFunctions[];
extern int __glXDispatchTableIndices[];
extern const char *__glXDispatchTableStrings[];

extern void __glXGLVNDInitDispatchFunctions(void);

#if defined(__cplusplus)
}
#endif

#endif
