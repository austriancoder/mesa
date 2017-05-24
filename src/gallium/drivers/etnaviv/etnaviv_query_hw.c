/*
 * Copyright (c) 2017 Etnaviv Project
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
 *    Rob Clark <robclark@freedesktop.org>
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include "util/u_inlines.h"
#include "util/u_memory.h"

#include "etnaviv_context.h"
#include "etnaviv_emit.h"
#include "etnaviv_query_hw.h"
#include "etnaviv_screen.h"

#define VIVS_GL_OCCLUSION_QUERY_ADDR                           0x00003824
#define VIVS_GL_OCCLUSION_QUERY                                0x00003830

static void
realloc_query_bo(struct etna_context *ctx, struct etna_hw_query *hq)
{
	struct etna_resource *rsc;
	void *map;

	pipe_resource_reference(&hq->prsc, NULL);

	hq->prsc = pipe_buffer_create(&ctx->screen->base,
			PIPE_BIND_QUERY_BUFFER, 0, 0x2);

	/* don't assume the buffer is zero-initialized: */
	rsc = etna_resource(hq->prsc);

	etna_bo_cpu_prep(rsc->bo, DRM_ETNA_PREP_WRITE);

	map = etna_bo_map(rsc->bo);
	memset(map, 0, 0x2);
	etna_bo_cpu_fini(rsc->bo);
}

static void
etna_hw_destroy_query(struct etna_context *ctx, struct etna_query *q)
{
   struct etna_hw_query *hq = etna_hw_query(q);

   pipe_resource_reference(&hq->prsc, NULL);
   FREE(hq);
}

static boolean
etna_hw_begin_query(struct etna_context *ctx, struct etna_query *q)
{
   struct etna_hw_query *hq = etna_hw_query(q);
   struct etna_reloc r = {
      .flags = ETNA_RELOC_WRITE
   };

   q->active = true;

   /* ->begin_query() discards previous results, so realloc bo: */
   realloc_query_bo(ctx, hq);

   /* resume */
   r.bo = etna_resource(hq->prsc)->bo;
   etna_set_state_reloc(ctx->stream, VIVS_GL_OCCLUSION_QUERY_ADDR, &r);

   return true;
}

static void
etna_hw_end_query(struct etna_context *ctx, struct etna_query *q)
{
   q->active = false;

   /* pause */
   etna_set_state(ctx->stream, VIVS_GL_OCCLUSION_QUERY, 0x1DF5E76);
}

static boolean
etna_hw_get_query_result(struct etna_context *ctx, struct etna_query *q,
                         boolean wait, union pipe_query_result *result)
{
   struct etna_hw_query *hq = etna_hw_query(q);
   struct etna_resource *rsc = etna_resource(hq->prsc);
   uint64_t *res64 = (uint64_t *)result;

   if (q->active)
      return false;

   /* if !wait, then check the last sample (the one most likely to
    * not be ready yet) and bail if it is not ready:
    */
   if (!wait) {
      int ret;

      ret = etna_bo_cpu_prep(rsc->bo, DRM_ETNA_PREP_READ | DRM_ETNA_PREP_NOSYNC);
      if (ret)
         return false;

      etna_bo_cpu_fini(rsc->bo);
   }

   /* get the result: */
   etna_bo_cpu_prep(rsc->bo, DRM_ETNA_PREP_READ);

   uint64_t *ptr = etna_bo_map(rsc->bo);
   *res64 = *ptr;
   etna_bo_cpu_fini(rsc->bo);

   return true;
}

static const struct etna_query_funcs hw_query_funcs = {
   .destroy_query = etna_hw_destroy_query,
   .begin_query = etna_hw_begin_query,
   .end_query = etna_hw_end_query,
   .get_query_result = etna_hw_get_query_result,
};

struct etna_query *
etna_hw_create_query(struct etna_context *ctx, unsigned query_type)
{
   struct etna_hw_query *hq;
   struct etna_query *q;

   if (query_type != PIPE_QUERY_OCCLUSION_COUNTER)
      return NULL;

   hq = CALLOC_STRUCT(etna_hw_query);
   if (!hq)
      return NULL;

   q = &hq->base;
   q->funcs = &hw_query_funcs;
   q->type = query_type;

   return q;
}
