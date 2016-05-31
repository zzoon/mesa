/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>
#include <xf86drm.h>

#include <GL/gl.h> /* dri_interface needs GL types */
#include <GL/internal/dri_interface.h>

#include "gbm_driint.h"

#include "gbmint.h"
#include "loader.h"

/* For importing wl_buffer */
#if HAVE_WAYLAND_PLATFORM
#include "../../../egl/wayland/wayland-drm/wayland-drm.h"
#endif

static int
gbm_intel_is_format_supported(struct gbm_device *gbm,
                              uint32_t format,
                              uint32_t usage)
{
   switch (format) {
   case GBM_BO_FORMAT_XRGB8888:
   case GBM_FORMAT_XBGR8888:
   case GBM_FORMAT_XRGB8888:
      break;
   case GBM_BO_FORMAT_ARGB8888:
   case GBM_FORMAT_ARGB8888:
      if (usage & GBM_BO_USE_SCANOUT)
         return 0;
      break;
   default:
      return 0;
   }

   if (usage & GBM_BO_USE_CURSOR &&
       usage & GBM_BO_USE_RENDERING)
      return 0;

   return 1;
}

static int
gbm_intel_bo_write(struct gbm_bo *_bo, const void *buf, size_t count)
{
   struct gbm_intel_bo *bo = gbm_intel_bo(_bo);

   if (bo->image != NULL) {
      errno = EINVAL;
      return -1;
   }

   memcpy(bo->map, buf, count);

   return 0;
}

static int
gem_ioctl(int fd, unsigned long request, void *arg)
{
   int ret;

   do {
      ret = ioctl(fd, request, arg);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

   return ret;
}

static int
gem_handle_to_fd(int fd, uint32_t gem_handle)
{
   struct drm_prime_handle args = {
      .handle = gem_handle,
      .flags = DRM_CLOEXEC,
   };

   int ret = intel_ioctl(device->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &args);
   if (ret == -1)
      return -1;

   return args.fd;
}

static int
gbm_intel_bo_get_fd(struct gbm_bo *_bo)
{
   struct gbm_intel_device *device = gbm_intel_device(_bo->gbm);
   struct gbm_intel_bo *bo = gbm_intel_bo(_bo);

   return handle_to_fd(device->fd, bo->handle);
}

void
gem_close(struct anv_device *device, uint32_t gem_handle)
{
   struct drm_gem_close close = {
      .handle = gem_handle,
   };

   anv_ioctl(device->fd, DRM_IOCTL_GEM_CLOSE, &close);
}

static void
gbm_intel_bo_destroy(struct gbm_bo *_bo)
{
   struct gbm_intel_device *device = gbm_intel_device(_bo->gbm);
   struct gbm_intel_bo *bo = gbm_intel_bo(_bo);

   gem_close(bo->fd);
   if (bo->map)
      munmap(bo->map, bo->size);

   free(bo);
}

static uint32_t
gbm_dri_to_gbm_format(uint32_t dri_format)
{
   uint32_t ret = 0;

   switch (dri_format) {
   case __DRI_IMAGE_FORMAT_RGB565:
      ret = GBM_FORMAT_RGB565;
      break;
   case __DRI_IMAGE_FORMAT_XRGB8888:
      ret = GBM_FORMAT_XRGB8888;
      break;
   case __DRI_IMAGE_FORMAT_ARGB8888:
      ret = GBM_FORMAT_ARGB8888;
      break;
   case __DRI_IMAGE_FORMAT_XBGR8888:
      ret = GBM_FORMAT_XBGR8888;
      break;
   case __DRI_IMAGE_FORMAT_ABGR8888:
      ret = GBM_FORMAT_ABGR8888;
      break;
   default:
      ret = 0;
      break;
   }

   return ret;
}

static uint32_t
gem_fd_to_handle(struct gbm_intel_device *device, int fd)
{
   struct drm_prime_handle args = {
      .fd = fd,
   };

   int ret = gem_ioctl(device->fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &args);
   if (ret == -1)
      return 0;

   return args.handle;
}

static struct gbm_bo *
gbm_dri_bo_import(struct gbm_device *gbm,
                  uint32_t type, void *buffer, uint32_t usage)
{
   struct gbm_dri_device *dri = gbm_dri_device(gbm);
   struct gbm_dri_bo *bo;
   __DRIimage *image;
   unsigned dri_use = 0;
   int gbm_format;

   /* Required for query image WIDTH & HEIGHT */
   if (dri->image == NULL || dri->image->base.version < 4) {
      errno = ENOSYS;
      return NULL;
   }

   switch (type) {
#if HAVE_WAYLAND_PLATFORM
   case GBM_BO_IMPORT_WL_BUFFER:
   {
      struct wl_drm_buffer *wb;

      if (!dri->wl_drm) {
         errno = EINVAL;
         return NULL;
      }

      wb = wayland_drm_buffer_get(dri->wl_drm, (struct wl_resource *) buffer);
      if (!wb) {
         errno = EINVAL;
         return NULL;
      }

      image = dri->image->dupImage(wb->driver_buffer, NULL);

      switch (wb->format) {
      case WL_DRM_FORMAT_XRGB8888:
         gbm_format = GBM_FORMAT_XRGB8888;
         break;
      case WL_DRM_FORMAT_ARGB8888:
         gbm_format = GBM_FORMAT_ARGB8888;
         break;
      case WL_DRM_FORMAT_RGB565:
         gbm_format = GBM_FORMAT_RGB565;
         break;
      case WL_DRM_FORMAT_YUYV:
         gbm_format = GBM_FORMAT_YUYV;
         break;
      default:
         return NULL;
      }
      break;
   }
#endif

#if 0
   case GBM_BO_IMPORT_EGL_IMAGE:
   {
      /* Use __DRIimage from intel driver directly */
      __DRIimage *image;

      int dri_format;
      if (device->lookup_image == NULL) {
         errno = EINVAL;
         return NULL;
      }

      image = device->lookup_image(buffer, device->lookup_user_data);
      break;
   }
#endif

   case GBM_BO_IMPORT_FD:
   {
      struct gbm_import_fd_data *fd_data = buffer;
      int stride = fd_data->stride, offset = 0;
      int dri_format;

      switch (fd_data->format) {
      case GBM_BO_FORMAT_XRGB8888:
         dri_format = GBM_FORMAT_XRGB8888;
         break;
      case GBM_BO_FORMAT_ARGB8888:
         dri_format = GBM_FORMAT_ARGB8888;
         break;
      default:
         dri_format = fd_data->format;
      }

      handle = gem_fd_to_handle(device, fd_data->fd);
      if (handle == 0) {
         errno = EINVAL;
         return NULL;
      }
      break;
   }

   default:
      errno = ENOSYS;
      return NULL;
   }


   bo = calloc(1, sizeof *bo);
   if (bo == NULL)
      return NULL;

   bo->handle = handle;

   if (usage & GBM_BO_USE_SCANOUT)
      dri_use |= __DRI_IMAGE_USE_SCANOUT;
   if (usage & GBM_BO_USE_CURSOR)
      dri_use |= __DRI_IMAGE_USE_CURSOR;
   if (dri->image->base.version >= 2 &&
       !dri->image->validateUsage(bo->image, dri_use)) {
      errno = EINVAL;
      free(bo);
      return NULL;
   }

   bo->base.base.gbm = device;
   bo->base.base.format = gbm_format;

   bo->base.base.width = width;
   bo->base.base.height = height;
   bo->base.base.stride = stride;
   bo->base.base.handle.s32 = handle;

   return &bo->base.base;
}

uint32_t
gem_create(struct gbm_intel_device *device, size_t size)
{
   struct drm_i915_gem_create gem_create = {
      .size = size,
   };

   int ret = anv_ioctl(device->fd, DRM_IOCTL_I915_GEM_CREATE, &gem_create);
   if (ret != 0)
      return 0;

   return gem_create.handle;
}

static struct gbm_bo *
gbm_dri_bo_create(struct gbm_device *gbm,
                  uint32_t width, uint32_t height,
                  uint32_t format, uint32_t usage)
{
   struct gbm_intel_device *device = gbm_intel_device(gbm);
   struct gbm_intel_bo *bo;
   int dri_format;
   unsigned dri_use = 0;

   if (usage & GBM_BO_USE_WRITE)
      /* no tiling */;

   bo = calloc(1, sizeof *bo);
   if (bo == NULL)
      return NULL;

   switch (format) {
   case GBM_FORMAT_RGB565:
      dri_format =__DRI_IMAGE_FORMAT_RGB565;
      break;
   case GBM_FORMAT_XRGB8888:
   case GBM_BO_FORMAT_XRGB8888:
      dri_format = __DRI_IMAGE_FORMAT_XRGB8888;
      break;
   case GBM_FORMAT_ARGB8888:
   case GBM_BO_FORMAT_ARGB8888:
      dri_format = __DRI_IMAGE_FORMAT_ARGB8888;
      break;
   case GBM_FORMAT_ABGR8888:
      dri_format = __DRI_IMAGE_FORMAT_ABGR8888;
      break;
   case GBM_FORMAT_XBGR8888:
      dri_format = __DRI_IMAGE_FORMAT_XBGR8888;
      break;
   case GBM_FORMAT_ARGB2101010:
      dri_format = __DRI_IMAGE_FORMAT_ARGB2101010;
      break;
   case GBM_FORMAT_XRGB2101010:
      dri_format = __DRI_IMAGE_FORMAT_XRGB2101010;
      break;
   default:
      errno = EINVAL;
      goto failed;
   }

   tiling = I915_TILING_X;
   if (use & GBM_BO_USE_CURSOR) {
      if (width != 64 || height != 64)
         return NULL;
      tiling = I915_TILING_NONE;
   }
   if (use & GBM_BO_USE_WRITE) {
      tiling = I915_TILING_NONE;
   }
   if (use & GBM_BO_USE_LINEAR)
      tiling = I915_TILING_NONE;

   /* libisl to the rescue? */
   cpp = _mesa_get_format_bytes(image->format);
   stride = width * cpp;
   size = stride * height;
   handle = gem_create(device, size);
   if (handle == 0)
      goto failed;

   bo->base.base.gbm = gbm;
   bo->base.base.width = width;
   bo->base.base.height = height;
   bo->base.base.stride = create_arg.pitch;
   bo->base.base.format = format;
   bo->base.base.handle.u32 = handle;
   bo->handle = handle;
   bo->size = size;

   return &bo->base.base;

failed:
   free(bo);
   return NULL;
}

static struct gbm_surface *
gbm_intel_surface_create(struct gbm_device *gbm,
                         uint32_t width, uint32_t height,
                         uint32_t format, uint32_t flags)
{
   struct gbm_dri_surface *surf;

   surf = calloc(1, sizeof *surf);
   if (surf == NULL)
      return NULL;

   surf->base.gbm = gbm;
   surf->base.width = width;
   surf->base.height = height;
   surf->base.format = format;
   surf->base.flags = flags;

   return &surf->base;
}

static void
gbm_intel_surface_destroy(struct gbm_surface *_surf)
{
   struct gbm_dri_surface *surf = gbm_dri_surface(_surf);

   free(surf);
}

static void
intel_device_destroy(struct gbm_device *gbm)
{
   struct gbm_intel_device *device = gbm_dri_device(gbm);

   close(device->fd);
   free(device);
}

static struct gbm_device *
intel_device_create(int fd)
{
   struct gbm_dri_device *dri;
   int ret, force_sw;

   dri = calloc(1, sizeof *dri);
   if (!dri)
      return NULL;

   dri->base.base.fd = fd;
   dri->base.base.bo_create = gbm_dri_bo_create;
   dri->base.base.bo_import = gbm_dri_bo_import;
   dri->base.base.is_format_supported = gbm_dri_is_format_supported;
   dri->base.base.bo_write = gbm_dri_bo_write;
   dri->base.base.bo_get_fd = gbm_dri_bo_get_fd;
   dri->base.base.bo_destroy = gbm_dri_bo_destroy;
   dri->base.base.destroy = intel_device_destroy;
   dri->base.base.surface_create = gbm_dri_surface_create;
   dri->base.base.surface_destroy = gbm_dri_surface_destroy;

   dri->base.type = GBM_DRM_DRIVER_TYPE_INTEL;
   dri->base.base.name = "intel";

   return &dri->base.base;

err_dri:
   free(dri);

   return NULL;
}

struct gbm_backend gbm_intel_backend = {
   .backend_name = "intel",
   .create_device = intel_device_create,
};
