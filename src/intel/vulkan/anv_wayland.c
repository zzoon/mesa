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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <wayland-client.h>
#include <wayland-drm-server-protocol.h>

#include "anv_private.h"

struct anv_wl_drm {
   struct wl_display *                          wl_display;
   struct wl_global *                           wl_drm_global;

   struct anv_device *                          device;
   const VkAllocationCallbacks*                 alloc;

   char *                                       device_name;
};

struct buffer_layout {
   uint32_t                                     fourcc;
   uint32_t                                     components;
   uint32_t                                     nplanes;
   struct image_plane {
      int buffer_index;
      int width_shift;
      int height_shift;
      VkFormat vk_format;
   }                                            planes[3];
};

#define COMPONENTS_RGB    0x3001
#define COMPONENTS_RGBA	  0x3002
#define COMPONENTS_Y_U_V  0x3003
#define COMPONENTS_Y_UV   0x3004
#define COMPONENTS_Y_XUXV 0x3005
#define COMPONENTS_R      0x3006
#define COMPONENTS_RG     0x3007

struct anv_wl_buffer {
   struct wl_resource *                         resource;
   VkExtent3D                                   extent;
   const struct buffer_layout *                 layout;
   struct anv_bo                                bo;
   VkImage                                      images[3];

   int refcount;
};

static void
buffer_deref(struct anv_wl_buffer *buffer)
{
   buffer->refcount--;
   if (buffer->refcount > 0)
      return;

   for (uint32_t i = 0; i < buffer->layout->nplanes; i++) {
      anv_DestroyImage(anv_device_to_handle(drm->device),
                       buffer->images[i], drm->alloc);
   }

   anv_gem_close(drm->device, buffer->bo.gem_handle);

   anv_free(drm->alloc, buffer);
}

static void
destroy_buffer(struct wl_resource *resource)
{
   buffer_deref(resource->data);
}

static void
buffer_destroy(struct wl_client *client, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct wl_buffer_interface buffer_interface = {
   .destroy = buffer_destroy
};

static void
drm_create_buffer(struct wl_client *client, struct wl_resource *resource,
                  uint32_t id, uint32_t name, int32_t width, int32_t height,
                  uint32_t stride, uint32_t format)
{
   wl_resource_post_error(resource,
                          WL_DRM_ERROR_INVALID_NAME,
                          "only prime buffers supported");
}

static void
drm_create_planar_buffer(struct wl_client *client,
                         struct wl_resource *resource,
                         uint32_t id, uint32_t name,
                         int32_t width, int32_t height, uint32_t format,
                         int32_t offset0, int32_t stride0,
                         int32_t offset1, int32_t stride1,
                         int32_t offset2, int32_t stride2)
{
   wl_resource_post_error(resource,
                          WL_DRM_ERROR_INVALID_NAME,
                          "only prime buffers supported");
}

static const struct buffer_layout buffer_layouts[] = {
   { WL_DRM_FORMAT_ARGB8888, COMPONENTS_RGBA, 1,
     { { 0, 0, 0, VK_FORMAT_B8G8R8A8_UNORM } } },

   { WL_DRM_FORMAT_ABGR8888, COMPONENTS_RGBA, 1,
     { { 0, 0, 0, VK_FORMAT_R8G8B8A8_UNORM } } },

   { WL_DRM_FORMAT_XRGB8888, COMPONENTS_RGB, 1,
     { { 0, 0, 0, VK_FORMAT_B8G8R8A8_UNORM }, } },

   { WL_DRM_FORMAT_XBGR8888, COMPONENTS_RGB, 1,
     { { 0, 0, 0, VK_FORMAT_R8G8B8A8_UNORM }, } },

   { WL_DRM_FORMAT_RGB565, COMPONENTS_RGB, 1,
     { { 0, 0, 0, VK_FORMAT_B5G6R5_UNORM_PACK16 } } },

   { WL_DRM_FORMAT_YUV410, COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, VK_FORMAT_R8_UNORM },
       { 1, 2, 2, VK_FORMAT_R8_UNORM },
       { 2, 2, 2, VK_FORMAT_R8_UNORM } } },

   { WL_DRM_FORMAT_YUV411, COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, VK_FORMAT_R8_UNORM },
       { 1, 2, 0, VK_FORMAT_R8_UNORM },
       { 2, 2, 0, VK_FORMAT_R8_UNORM } } },

   { WL_DRM_FORMAT_YUV420, COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, VK_FORMAT_R8_UNORM },
       { 1, 1, 1, VK_FORMAT_R8_UNORM },
       { 2, 1, 1, VK_FORMAT_R8_UNORM } } },

   { WL_DRM_FORMAT_YUV422, COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, VK_FORMAT_R8_UNORM },
       { 1, 1, 0, VK_FORMAT_R8_UNORM },
       { 2, 1, 0, VK_FORMAT_R8_UNORM } } },

   { WL_DRM_FORMAT_YUV444, COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, VK_FORMAT_R8_UNORM },
       { 1, 0, 0, VK_FORMAT_R8_UNORM },
       { 2, 0, 0, VK_FORMAT_R8_UNORM } } },

   { WL_DRM_FORMAT_NV12, COMPONENTS_Y_UV, 2,
     { { 0, 0, 0, VK_FORMAT_R8_UNORM },
       { 1, 1, 1, VK_FORMAT_R8G8_UNORM } } },

   { WL_DRM_FORMAT_NV16, COMPONENTS_Y_UV, 2,
     { { 0, 0, 0, VK_FORMAT_R8_UNORM },
       { 1, 1, 0, VK_FORMAT_R8G8_UNORM } } },

   /* For YUYV buffers, we set up two overlapping DRI images and treat them as
    * planar buffers in the compositors.  Plane 0 is RG88 and samples YU or YV
    * pairs and places Y into the R component, while plane 1 is ARGB8888 and
    * samples YUYV clusters and places U into the G component and V into A.
    * This lets the texture sampler filter the Y components correctly when
    * sampling from plane 0, and filter U and V correctly when sampling from
    * plane 1. */
   { WL_DRM_FORMAT_YUYV, COMPONENTS_Y_XUXV, 2,
     { { 0, 0, 0, VK_FORMAT_R8G8_UNORM },
       { 0, 1, 0, VK_FORMAT_B8G8R8A8_UNORM } } }
};

static const struct buffer_layout *
lookup_layout(uint32_t wl_format)
{
   for (uint32_t i = 0; i < ARRAY_SIZE(buffer_layouts); i++) {
      if (buffer_layouts[i].fourcc == wl_format)
         return &buffer_layouts[i];
   }

   return NULL;
}

static void
drm_create_prime_buffer(struct wl_client *client,
                        struct wl_resource *resource,
                        uint32_t id, int fd,
                        int32_t width, int32_t height, uint32_t format,
                        int32_t offset0, int32_t stride0,
                        int32_t offset1, int32_t stride1,
                        int32_t offset2, int32_t stride2)
{
   struct anv_wl_drm *drm = resource->data;
   struct anv_wl_buffer *buffer;
   const struct buffer_layout *layout;

   if (width <= 0 || height <= 0) {
      wl_resource_post_error(resource,
                             WL_DRM_ERROR_INVALID_FORMAT,
                             "invalid extent");
      goto fail_fd;
   }

   layout = lookup_layout(format);
   if (layout == NULL) {
      wl_resource_post_error(resource,
                             WL_DRM_ERROR_INVALID_FORMAT,
                             "invalid format");
      goto fail_fd;
   }

   buffer = anv_alloc(drm->alloc, sizeof(*buffer), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (buffer == NULL) {
      wl_resource_post_no_memory(resource);
      goto fail_fd;
   }

   buffer->extent.width = width;
   buffer->extent.height = height;
   buffer->extent.depth = 1;
   buffer->layout = layout;

   buffer->bo.gem_handle = anv_gem_fd_to_handle(drm->device, fd);
   if (!buffer->bo.gem_handle) {
      wl_resource_post_error(resource,
                             WL_DRM_ERROR_INVALID_NAME,
                             "invalid name");
      goto fail_buffer;
   }

   int size;
   size = lseek(fd, 0, SEEK_END);
   if (size == -1) {
      wl_resource_post_error(resource,
                             WL_DRM_ERROR_INVALID_NAME,
                             "invalid name");
      goto fail_handle;
   }

   buffer->bo.map = NULL;
   buffer->bo.index = 0;
   buffer->bo.offset = 0;
   buffer->bo.size = size;

   buffer->resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);
   if (!buffer->resource) {
      wl_resource_post_no_memory(resource);
      goto fail_handle;
   }

   wl_resource_set_implementation(buffer->resource,
                                  (void (**)(void)) &buffer_interface,
                                  buffer, destroy_buffer);

   const struct {
      int32_t offset;
      int32_t stride;
   } planes[3] = {
      { offset0, stride0 },
      { offset1, stride1 },
      { offset2, stride2 }
   };

   uint32_t count = 0;
   for (uint32_t i = 0; i < layout->nplanes; i++) {
      const struct image_plane plane = buffer->layout->planes[i];
      struct anv_image *image;

      const VkExtent3D extent = {
         .width = buffer->extent.width >> plane.width_shift,
         .height = buffer->extent.height >> plane.height_shift,
         .depth = 1
      };

      VkResult result =
         anv_image_create(anv_device_to_handle(drm->device),
         &(struct anv_image_create_info) {
            .isl_tiling_flags = ISL_TILING_X_BIT,
            .stride = planes[plane.buffer_index].stride,
            .vk_info =
               &(VkImageCreateInfo) {
                  .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                  .imageType = VK_IMAGE_TYPE_2D,
                  .format = plane.vk_format,
                  .extent = extent,
                  .mipLevels = 1,
                  .arrayLayers = 1,
                  .samples = 1,
                  .tiling = VK_IMAGE_TILING_OPTIMAL,
                  .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                  .flags = 0,
         }}, drm->alloc, &buffer->images[i]);

      if (result != VK_SUCCESS)
         goto fail_images;

      image = anv_image_from_handle(buffer->images[i]);
      image->bo = &buffer->bo;
      image->offset = planes[plane.buffer_index].offset;
      count++;
   }

   close(fd);
   return;

 fail_images:
   for (uint32_t i = 0; i < count; i++)
      anv_DestroyImage(anv_device_to_handle(drm->device),
                       buffer->images[i], drm->alloc);
 fail_handle:
   anv_gem_close(drm->device, buffer->bo.gem_handle);
 fail_buffer:
   anv_free(drm->alloc, buffer);
 fail_fd:
   close(fd);
   return;
}

static void
drm_authenticate(struct wl_client *client,
                 struct wl_resource *resource, uint32_t id)
{
   wl_resource_post_error(resource,
                          WL_DRM_ERROR_AUTHENTICATE_FAIL,
                          "authenicate not supported");
}

static const struct wl_drm_interface drm_interface = {
   drm_authenticate,
   drm_create_buffer,
   drm_create_planar_buffer,
   drm_create_prime_buffer
};

static void
bind_drm(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   struct anv_wl_drm *drm = data;
   struct wl_resource *resource;
   uint32_t capabilities;

   resource = wl_resource_create(client, &wl_drm_interface,
                                 MIN(version, 2), id);
   if (!resource) {
      wl_client_post_no_memory(client);
      return;
   }

   wl_resource_set_implementation(resource, &drm_interface, data, NULL);

   wl_resource_post_event(resource, WL_DRM_DEVICE, drm->device_name);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_ARGB8888);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_XRGB8888);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_RGB565);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_YUV410);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_YUV411);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_YUV420);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_YUV422);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_YUV444);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_NV12);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_NV16);
   wl_resource_post_event(resource, WL_DRM_FORMAT, WL_DRM_FORMAT_YUYV);

   capabilities = WL_DRM_CAPABILITY_PRIME;
   if (version >= 2)
      wl_resource_post_event(resource, WL_DRM_CAPABILITIES, capabilities);
}

VkResult anv_BindDisplayWL(
    VkDevice                                    _device,
    const VkAllocationCallbacks*                pAllocator,
    struct wl_display *                         wl_dpy)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_wl_drm *drm;

   drm = anv_alloc2(&device->alloc, pAllocator, sizeof(*drm), 8,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (drm == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   drm->device = device;
   if (pAllocator)
      drm->alloc = pAllocator;
   else
      drm->alloc = &device->alloc;
   drm->wl_display = wl_dpy;

   drm->wl_drm_global =
      wl_global_create(wl_dpy, &wl_drm_interface, 2, drm, bind_drm);

   return VK_SUCCESS;
}



VkResult anv_LockBufferImagesWL(
   VkDevice                                    _device,
   struct wl_resource *                        buffer_resource,
   uint32_t *                                  pImageCount,
   VkBufferImageWL *                           pImages,
   VkExtent3D *                                extent)
{
   struct anv_wl_buffer *buffer;

   /* FIXME: Need semaphores, and need to communicate formats and how
    * components maps to images. y-inverted?
    */

   if (!wl_resource_instance_of(buffer_resource, &wl_buffer_interface,
                                &buffer_interface)) {
      *pImageCount = 0;
      return VK_SUCCESS;
   }

   buffer = wl_resource_get_user_data(buffer_resource);

   *pImageCount = buffer->layout->nplanes;
   if (pImages == NULL)
      return VK_SUCCESS;

   for (uint32_t i = 0; i < buffer->layout->nplanes; i++) {
      pImages[i].image = buffer->images[i];
      pImages[i].format = buffer->layout->planes[i].vk_format;
   }

   *extent = buffer->extent;

   buffer->refcount++;

   return VK_SUCCESS;
}

void anv_ReleaseBufferImagesWL(
   VkDevice                                    _device,
   struct wl_resource *                        buffer_resource)
{
   struct anv_wl_buffer *buffer;

   assert(wl_resource_instance_of(buffer_resource, &wl_buffer_interface,
                                  &buffer_interface));

   buffer = wl_resource_get_user_data(buffer_resource);

   buffer_deref(buffer);
}
