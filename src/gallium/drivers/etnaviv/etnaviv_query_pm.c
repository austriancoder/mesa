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

static void
etna_pm_destroy_query(struct etna_context *ctx, struct etna_query *q)
{
   struct etna_pm_query *pq = etna_pm_query(q);

   FREE(pq);
}

static boolean
etna_pm_begin_query(struct etna_context *ctx, struct etna_query *q)
{
   q->active = true;

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
   if (q->active)
      return false;

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
   struct etna_pm_query *pq;
   struct etna_query *q;

   pq = CALLOC_STRUCT(etna_pm_query);
   if (!pq)
      return NULL;

   q = &pq->base;
   q->funcs = &hw_query_funcs;
   q->type = query_type;

   return q;
}
