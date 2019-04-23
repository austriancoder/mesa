/*
 * Copyright (c) 2019 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include "etnaviv_context.h"
#include "etnaviv_emit.h"
#include "etnaviv_yuv.h"
#include "util/u_format.h"

bool
etna_format_needs_yuv_tiler(enum pipe_format format)
{
   bool needs_tiler;

   switch (format) {
      case PIPE_FORMAT_NV12:
         needs_tiler = true;
         break;

      default:
         needs_tiler = false;
         break;
   }

   return needs_tiler;
}

static void
emit_plane(struct etna_context *ctx, struct etna_resource *plane,
           enum etna_resource_status status, uint32_t base, uint32_t stride)
{
   if (!plane)
      return;

   etna_resource_used(ctx, &plane->base, status);

   etna_set_state_reloc(ctx->stream, base, &(struct etna_reloc) {
      .bo = plane->bo,
      .offset = plane->levels[0].offset,
      .flags = ETNA_RELOC_READ,
   });
   etna_set_state(ctx->stream, stride, plane->levels[0].stride);
}

bool
etna_try_yuv_blit(struct pipe_context *pctx,
                  const struct pipe_blit_info *blit_info)
{
   struct etna_context *ctx = etna_context(pctx);
   struct etna_cmd_stream *stream = ctx->stream;
   struct pipe_resource *src = blit_info->src.resource;
   struct etna_resource *dst = etna_resource(blit_info->dst.resource);
   struct etna_resource *planes[3] = { 0 };
   int idx = 0;
   unsigned num_planes;
   uint32_t format;

   assert(util_format_is_yuv(blit_info->src.format));
   assert(blit_info->dst.format == PIPE_FORMAT_YUYV);
   assert(blit_info->src.level == 0);
   assert(blit_info->dst.level == 0);

   switch (blit_info->src.format) {
   case PIPE_FORMAT_NV12:
      format = 0x1;
      num_planes = 2;
      break;
   default:
      return false;
   }

   while (src) {
      planes[idx++] = etna_resource(src);
      src = src->next;
   }

   assert(idx == num_planes);

   etna_set_state(stream, VIVS_YUV_CONFIG,
                  VIVS_YUV_CONFIG_SOURCE_FORMAT(format) | VIVS_YUV_CONFIG_ENABLE);
   etna_set_state(stream, VIVS_YUV_WINDOW_SIZE,
                  VIVS_YUV_WINDOW_SIZE_HEIGHT(blit_info->dst.box.height) |
                  VIVS_YUV_WINDOW_SIZE_WIDTH(blit_info->dst.box.width));

   emit_plane(ctx, planes[0], ETNA_PENDING_READ, VIVS_YUV_Y_BASE, VIVS_YUV_Y_STRIDE);
   emit_plane(ctx, planes[1], ETNA_PENDING_READ, VIVS_YUV_U_BASE, VIVS_YUV_U_STRIDE);
   emit_plane(ctx, planes[2], ETNA_PENDING_READ, VIVS_YUV_V_BASE, VIVS_YUV_V_STRIDE);
   emit_plane(ctx, dst, ETNA_PENDING_WRITE, VIVS_YUV_DEST_BASE, VIVS_YUV_DEST_STRIDE);

   /* configure RS */
   etna_set_state(stream, VIVS_RS_SOURCE_STRIDE, 0);
   etna_set_state(stream, VIVS_RS_CLEAR_CONTROL, 0);

   /* trigger resolve */
   etna_set_state(stream, VIVS_RS_KICKER, 0xbadabeeb);

   /* disable yuv tiller */
   etna_set_state(stream, VIVS_YUV_CONFIG, 0x0);

   return true;
}
