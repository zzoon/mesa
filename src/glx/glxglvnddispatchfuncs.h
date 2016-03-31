#ifndef __glx_glvnd_dispatch_funcs_h__
#define __glx_glvnd_dispatch_funcs_h__
/*
 * Helper functions used by g_glxglvnddispatchfuncs.c.
 */
#include "glxglvnd.h"
#include <assert.h>

#define __VND __glXGLVNDAPIExports

#define GET_CURRENT_DISPLAY() glXGetCurrentDisplay()

static inline Display *GET_DEFAULT_DISPLAY(void)
{
    // TODO think of a better heuristic...
    assert(!"GET_DEFAULT_DISPLAY() called");
    return GET_CURRENT_DISPLAY();
}

static inline int AddFBConfigMapping(Display *dpy, GLXFBConfig config,
                                     __GLXvendorInfo *vendor)
{
    return __VND.addVendorFBConfigMapping(dpy, config, vendor);
}

static inline int AddFBConfigsMapping(Display *dpy, const GLXFBConfig *ret,
                                      int *nelements, __GLXvendorInfo *vendor)
{
    int i, r;

    if (!nelements || !ret)
        return 0;

    for (i = 0; i < *nelements; i++) {
        r = __VND.addVendorFBConfigMapping(dpy, ret[i], vendor);
        if (r) {
            for (; i >= 0; i--)
                __VND.removeVendorFBConfigMapping(dpy, ret[i]);
            break;
        }
    }
    return r;
}

static inline void AddVisualMapping(Display *dpy, const XVisualInfo *visual,
                                    __GLXvendorInfo *vendor)
{
    __VND.addScreenVisualMapping(dpy, visual, vendor);
}

static inline int AddDrawableMapping(Display *dpy, GLXDrawable drawable,
                                     __GLXvendorInfo *vendor)
{
    return __VND.addVendorDrawableMapping(dpy, drawable, vendor);
}

static inline int AddContextMapping(Display *dpy, GLXContext ctx,
                                    __GLXvendorInfo *vendor)
{
    return __VND.addVendorContextMapping(dpy, ctx, vendor);
}

static inline void GetDispatchFromDrawable(Display *dpy, GLXDrawable drawable,
                                           __GLXvendorInfo **retVendor)
{
    __VND.vendorFromDrawable(dpy, drawable, retVendor);
}

static inline void GetDispatchFromContext(Display *dpy, GLXContext ctx,
                                          __GLXvendorInfo **retVendor)
{
    __VND.vendorFromContext(ctx, retVendor);
}

static inline void GetDispatchFromFBConfig(Display *dpy, GLXFBConfig config,
                                           __GLXvendorInfo **retVendor)
{
    __VND.vendorFromFBConfig(dpy, config, retVendor);
}

static inline void GetDispatchFromVisual(Display *dpy, const XVisualInfo *visual,
                                         __GLXvendorInfo **retVendor)
{
    __VND.vendorFromVisual(dpy, visual, retVendor);
}

#endif // __glx_glvnd_dispatch_funcs_h__
