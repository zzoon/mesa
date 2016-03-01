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
                                      int screen, __GLXvendorInfo *vendor)
{
    __VND.addScreenFBConfigMapping(dpy, config, screen, vendor);
}

static inline void AddScreenFBConfigsMapping(Display *dpy, const GLXFBConfig *ret,
                                             int *nelements,
                                             int screen, __GLXvendorInfo *vendor)
{
    int i;

    if (!nelements || !ret)
        return;

    for (i = 0; i < *nelements; i++)
        __VND.addScreenFBConfigMapping(dpy, ret[i], screen, vendor);
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
                                     int screen, __GLXvendorInfo *vendor)
{
    __VND.addScreenContextMapping(dpy, ctx, screen, vendor);
}

static inline void GetDispatchFromDrawable(Display *dpy, GLXDrawable drawable,
                                           __GLXvendorInfo **retVendor)
{
    __VND.vendorFromDrawable(dpy, drawable, retVendor);
}

static inline void GetDispatchFromContext(Display *dpy, GLXContext ctx,
                                          int *retScreen, __GLXvendorInfo **retVendor)
{
    __VND.vendorFromContext(ctx, NULL, retScreen, retVendor);
}

static inline void GetDispatchFromFBConfig(Display *dpy, GLXFBConfig config,
                                           int *retScreen, __GLXvendorInfo **retVendor)
{
    __VND.vendorFromFBConfig(dpy, config, retScreen, retVendor);
}

static inline void GetDispatchFromVisual(Display *dpy, const XVisualInfo *visual,
                                         __GLXvendorInfo **retVendor)
{
    __VND.vendorFromVisual(dpy, visual, retVendor);
}

#endif // __glx_glvnd_dispatch_funcs_h__
