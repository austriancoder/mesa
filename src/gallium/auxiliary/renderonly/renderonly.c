/*
 * Copyright (C) 2016 Christian Gmeiner <christian.gmeiner@gmail.com>
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include "renderonly/renderonly.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <xf86drm.h>

#include "state_tracker/drm_driver.h"
#include "pipe/p_screen.h"
#include "util/u_memory.h"

struct pipe_screen *
renderonly_screen_create(int fd, const struct renderonly_ops *ops, void *priv)
{
   struct renderonly *ro;

   ro = CALLOC_STRUCT(renderonly);
   if (!ro)
      return NULL;

   ro->kms_fd = fd;
   ro->ops = ops;
   ro->priv = priv;

   ro->screen = ops->create(ro);
   if (!ro->screen)
      goto cleanup;

   return ro->screen;

cleanup:
   FREE(ro);

   return NULL;
}

static bool
import_gpu_scanout(struct renderonly_scanout *scanout,
      struct pipe_resource *rsc, struct renderonly *ro)
{
   struct pipe_screen *screen = ro->screen;
   boolean status;
   int fd, err;
   struct winsys_handle handle = {
      .type = DRM_API_HANDLE_TYPE_FD
   };

   status = screen->resource_get_handle(screen, rsc, &handle,
         PIPE_HANDLE_USAGE_READ_WRITE);
   if (!status)
      return false;

   scanout->stride = handle.stride;
   fd = handle.handle;

   err = drmPrimeFDToHandle(ro->kms_fd, fd, &scanout->handle);
   close(fd);

   if (err < 0) {
      fprintf(stderr, "drmPrimeFDToHandle() failed: %s\n", strerror(errno));
      return false;
   }

   if (ro->ops->tiling) {
      err = ro->ops->tiling(ro->kms_fd, scanout->handle);
      if (err < 0) {
         fprintf(stderr, "failed to set tiling parameters: %s\n",
               strerror(errno));
         close(scanout->handle);
         return false;
      }
   }

   return true;
}

struct renderonly_scanout *
renderonly_scanout_for_resource(struct pipe_resource *rsc, struct renderonly *ro)
{
   struct renderonly_scanout *scanout;
   bool ret;

   scanout = CALLOC_STRUCT(renderonly_scanout);
   if (!scanout)
      return NULL;

   ret = import_gpu_scanout(scanout, rsc, ro);

   if (!ret) {
      FREE(scanout);
      scanout = NULL;
   }

   return scanout;
}

void
renderonly_scanout_destroy(struct renderonly_scanout *scanout)
{
   close(scanout->handle);
   FREE(scanout);
}
