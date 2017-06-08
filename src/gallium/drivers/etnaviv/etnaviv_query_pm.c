/*
 * Copyright (c) 2017 Etnaviv Project
 * Copyright (C) 2017 Zodiac Inflight Innovations
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
#include "etnaviv_query_pm.h"
#include "etnaviv_screen.h"

struct etna_perfmon_config
{
  unsigned type;
  const char *domain;
  const char *signal;
};

static const struct etna_perfmon_config query_config[] = {
};

static const struct etna_perfmon_config *
etna_pm_query_config(unsigned type)
{
  for (unsigned i = 0; i < ARRAY_SIZE(query_config); i++)
    if (query_config[i].type == type)
      return &query_config[i];

  return NULL;
}

static struct etna_perfmon_signal *
etna_pm_query_signal(struct etna_perfmon *perfmon,
                     const struct etna_perfmon_config *cfg)
{
  struct etna_perfmon_domain *domain;

  domain = etna_perfmon_get_dom_by_name(perfmon, cfg->domain);
  if (!domain)
    return NULL;

  return etna_perfmon_get_sig_by_name(domain, cfg->signal);
}

static bool
realloc_query_bo(struct etna_context *ctx, struct etna_pm_query *pq)
{
   if (pq->bo)
      etna_bo_del(pq->bo);

   pq->bo = etna_bo_new(ctx->screen->dev, 64, DRM_ETNA_GEM_CACHE_WC);
   if (unlikely(!pq->bo))
      return false;

   pq->data = etna_bo_map(pq->bo);

   return true;
}

static void
etna_pm_query_get(struct etna_cmd_stream *stream, struct etna_query *q,
                  unsigned offset, unsigned flags)
{
   assert(offset);
   assert(flags);

   struct etna_pm_query *pq = etna_pm_query(q);
   struct etna_perf p = {
     .flags = flags,
     .sequence = pq->sequence,
     .signal = pq->signal,
     .bo = pq->bo,
     .offset = offset
   };

   etna_cmd_stream_perf(stream, &p);
}

static inline void
etna_pm_query_update(struct etna_query *q)
{
   struct etna_pm_query *pq = etna_pm_query(q);

   if (pq->data[0] == pq->sequence)
      pq->ready = true;
}

static void
etna_pm_destroy_query(struct etna_context *ctx, struct etna_query *q)
{
   struct etna_pm_query *pq = etna_pm_query(q);

   etna_bo_del(pq->bo);
   FREE(pq);
}

static boolean
etna_pm_begin_query(struct etna_context *ctx, struct etna_query *q)
{
   struct etna_pm_query *pq = etna_pm_query(q);

   q->active = true;
   pq->ready = false;
   pq->sequence++;

   switch (q->type) {
   default:
      assert(0); /* can't happen, we don't create queries with invalid type */
      break;
   }

   return true;
}

static void
etna_pm_end_query(struct etna_context *ctx, struct etna_query *q)
{
   q->active = false;

   switch (q->type) {
   default:
      break;
   }
}

static boolean
etna_pm_get_query_result(struct etna_context *ctx, struct etna_query *q,
                         boolean wait, union pipe_query_result *result)
{
   struct etna_pm_query *pq = etna_pm_query(q);

   if (q->active)
      return false;

   /* TODO: handle wait */

   if (!pq->ready) {
      etna_pm_query_update(q);
      return false;
    }

   util_query_clear_result(result, q->type);

   switch (q->type) {
   default:
      assert(0); /* can't happen, we don't create queries with invalid type */
      return false;
   }

   return true;
}

static const struct etna_query_funcs hw_query_funcs = {
   .destroy_query = etna_pm_destroy_query,
   .begin_query = etna_pm_begin_query,
   .end_query = etna_pm_end_query,
   .get_query_result = etna_pm_get_query_result,
};

struct etna_query *
etna_pm_create_query(struct etna_context *ctx, unsigned query_type)
{
   struct etna_perfmon *perfmon = ctx->screen->perfmon;
   const struct etna_perfmon_config *cfg;
   struct etna_perfmon_signal *signal;
   struct etna_pm_query *pq;
   struct etna_query *q;

   cfg = etna_pm_query_config(query_type);
   if (!cfg)
      return NULL;

   /* check if perf signal is supported by hardware */
   signal = etna_pm_query_signal(perfmon, cfg);
   if (!signal)
      return NULL;

   pq = CALLOC_STRUCT(etna_pm_query);
   if (!pq)
      return NULL;

   if (!realloc_query_bo(ctx, pq)) {
      FREE(pq);
      return NULL;
   }

   q = &pq->base;
   q->funcs = &hw_query_funcs;
   q->type = query_type;
   pq->signal = signal;

   return q;
}
