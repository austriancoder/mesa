/*
 * Copyright (c) 2016 Etnaviv Project
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

#include "os/os_time.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_string.h"

#include "etnaviv_context.h"
#include "etnaviv_query_hw.h"

static void
etna_hw_destroy_query(struct etna_context *ctx, struct etna_query *q)
{
   struct etna_hw_query *hq = etna_hw_query(q);

   FREE(hq);
}

static boolean
etna_hw_begin_query(struct etna_context *ctx, struct etna_query *q)
{
   struct etna_hw_query *hq = etna_hw_query(q);

   q->active = true;

   return true;
}

static void
etna_hw_end_query(struct etna_context *ctx, struct etna_query *q)
{
   struct etna_hw_query *hq = etna_hw_query(q);

   q->active = false;
}

static boolean
etna_hw_get_query_result(struct etna_context *ctx, struct etna_query *q,
                         boolean wait, union pipe_query_result *result)
{
   struct etna_hw_query *hq = etna_hw_query(q);

   if (q->active)
      return false;

   util_query_clear_result(result, q->type);

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

   switch (query_type) {
   default:
      return NULL;
   }

   hq = CALLOC_STRUCT(etna_hw_query);
   if (!hq)
      return NULL;

   q = &hq->base;
   q->funcs = &hw_query_funcs;
   q->type = query_type;

   return q;
}
