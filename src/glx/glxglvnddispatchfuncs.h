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

static inline void AddFBConfigMapping(Display *dpy, GLXFBConfig config,
                                      __GLXvendorInfo *vendor)
{
    __VND.addVendorFBConfigMapping(dpy, config, vendor);
}

static inline void AddFBConfigsMapping(Display *dpy, const GLXFBConfig *ret,
                                             int *nelements,
                                             __GLXvendorInfo *vendor)
{
    int i;

    if (!nelements || !ret)
        return;

    for (i = 0; i < *nelements; i++)
        __VND.addVendorFBConfigMapping(dpy, ret[i], vendor);
}

static inline void AddVisualMapping(Display *dpy, const XVisualInfo *visual,
                                    __GLXvendorInfo *vendor)
{
    __VND.addScreenVisualMapping(dpy, visual, vendor);
}

static inline void AddDrawableMapping(Display *dpy, GLXDrawable drawable,
                                      __GLXvendorInfo *vendor)
{
    __VND.addVendorDrawableMapping(dpy, drawable, vendor);
}

static inline void AddContextMapping(Display *dpy, GLXContext ctx,
                                     __GLXvendorInfo *vendor)
{
    __VND.addVendorContextMapping(dpy, ctx, vendor);
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
