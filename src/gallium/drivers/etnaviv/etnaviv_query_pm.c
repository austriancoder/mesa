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

struct etna_perfmon_source
{
   unsigned int offset;
   const char *domain;
   const char *signal;
};

struct etna_perfmon_config
{
   const char *name;
   unsigned type;
   unsigned nr_sources;
   const struct etna_perfmon_source *source;
};

static const struct etna_perfmon_config query_config[] = {
   {
      .name = "hi-total-cyles",
      .type = ETNA_QUERY_HI_TOTAL_CYCLES,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "HI", "TOTAL_CYCLES" }
      }
   },
   {
      .name = "hi-idle-cyles",
      .type = ETNA_QUERY_HI_IDLE_CYCLES,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "HI", "IDLE_CYCLES" }
      }
   },
   {
      .name = "hi-axi-cycles-read-request-stalled",
      .type = ETNA_QUERY_HI_AXI_CYCLES_READ_REQUEST_STALLED,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "HI", "AXI_CYCLES_READ_REQUEST_STALLED" }
      }
   },
   {
      .name = "hi-axi-cycles-write-request-stalled",
      .type = ETNA_QUERY_HI_AXI_CYCLES_WRITE_REQUEST_STALLED,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "HI", "AXI_CYCLES_WRITE_REQUEST_STALLED" }
      }
   },
   {
      .name = "hi-axi-cycles-write-data-stalled",
      .type = ETNA_QUERY_HI_AXI_CYCLES_WRITE_DATA_STALLED,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "HI", "AXI_CYCLES_WRITE_DATA_STALLED" }
      }
   },
   {
      .name = "pe-pixel-count-killed-by-color-pipe",
      .type = ETNA_QUERY_PE_PIXEL_COUNT_KILLED_BY_COLOR_PIPE,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "PE", "PIXEL_COUNT_KILLED_BY_COLOR_PIPE" }
      }
   },
   {
      .name = "pe-pixel-count-killed-by-depth-pipe",
      .type = ETNA_QUERY_PE_PIXEL_COUNT_KILLED_BY_DEPTH_PIPE,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "PE", "PIXEL_COUNT_KILLED_BY_DEPTH_PIPE" }
      }
   },
   {
      .name = "pe-pixel-count-drawn-by-color-pipe",
      .type = ETNA_QUERY_PE_PIXEL_COUNT_DRAWN_BY_COLOR_PIPE,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "PE", "PIXEL_COUNT_DRAWN_BY_COLOR_PIPE" }
      }
   },
   {
      .name = "pe-pixel-count-drawn-by-depth-pipe",
      .type = ETNA_QUERY_PE_PIXEL_COUNT_DRAWN_BY_COLOR_PIPE,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "PE", "PIXEL_COUNT_DRAWN_BY_DEPTH_PIPE" }
      }
   },
   {
      .name = "pe-pixels-rendered-2d",
      .type = ETNA_QUERY_PE_PIXEL_COUNT_DRAWN_BY_COLOR_PIPE,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "PE", "PIXELS_RENDERED_2D" }
      }
   },
   {
      .name = "sh-shader-cycles",
      .type = ETNA_QUERY_SH_SHADER_CYCLES,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "SHADER_CYCLES" }
      }
   },
   {
      .name = "sh-ps-inst-counter",
      .type = ETNA_QUERY_SH_PS_INST_COUNTER,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "PS_INST_COUNTER" }
      }
   },
   {
      .name = "sh-rendered-pixel-counter",
      .type = ETNA_QUERY_SH_RENDERED_PIXEL_COUNTER,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "RENDERED_PIXEL_COUNTER" }
      }
   },
   {
      .name = "sh-vs-inst-counter",
      .type = ETNA_QUERY_SH_VS_INST_COUNTER,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "VS_INST_COUNTER" }
      }
   },
   {
      .name = "sh-rendered-vertice-counter",
      .type = ETNA_QUERY_SH_RENDERED_VERTICE_COUNTER,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "RENDERED_VERTICE_COUNTER" }
      }
   },
   {
      .name = "sh-vtx-branch-inst-counter",
      .type = ETNA_QUERY_SH_RENDERED_VERTICE_COUNTER,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "VTX_BRANCH_INST_COUNTER" }
      }
   },
   {
      .name = "sh-vtx-texld-inst-counter",
      .type = ETNA_QUERY_SH_RENDERED_VERTICE_COUNTER,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "VTX_TEXLD_INST_COUNTER" }
      }
   },
   {
      .name = "sh-plx-branch-inst-counter",
      .type = ETNA_QUERY_SH_RENDERED_VERTICE_COUNTER,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "PXL_BRANCH_INST_COUNTER" }
      }
   },
   {
      .name = "sh-plx-texld-inst-counter",
      .type = ETNA_QUERY_SH_RENDERED_VERTICE_COUNTER,
      .nr_sources = 1,
      .source = (const struct etna_perfmon_source[]) {
         { 1, "SH", "PXL_TEXLD_INST_COUNTER" }
      }
   }
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
                     const struct etna_perfmon_source *source)
{
  struct etna_perfmon_domain *domain;

  domain = etna_perfmon_get_dom_by_name(perfmon, source->domain);
  if (!domain)
    return NULL;

  return etna_perfmon_get_sig_by_name(domain, source->signal);
}

static bool
etna_pm_cfg_supported(struct etna_perfmon *perfmon,
                                 const struct etna_perfmon_config *cfg)
{
   for (unsigned i = 0; i < cfg->nr_sources; i++) {
      struct etna_perfmon_signal *signal = etna_pm_query_signal(perfmon, &cfg->source[i]);

      if (!signal)
         return false;
   }

   return true;
}

static bool
etna_pm_add_signals(struct etna_pm_query *pq, struct etna_perfmon *perfmon,
                    const struct etna_perfmon_config *cfg)
{
   for (unsigned i = 0; i < cfg->nr_sources; i++) {
      struct etna_perfmon_signal *signal = etna_pm_query_signal(perfmon, &cfg->source[i]);
      struct etna_pm_source *source = CALLOC_STRUCT(etna_pm_source);

      if (!source)
         return false;

      source->signal = signal;
      source->offset = cfg->source[i].offset;

      list_addtail(&source->node, &pq->signals);
   }

   return true;
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
                  unsigned flags)
{
   struct etna_pm_query *pq = etna_pm_query(q);
   unsigned offset = 0;
   assert(flags);

   list_for_each_entry(struct etna_pm_source, source, &pq->signals, node) {
      struct etna_perf p = {
        .flags = flags,
        .sequence = pq->sequence,
        .bo = pq->bo,
        .signal = source->signal,
        .offset = pq->offset + source->offset
      };

      etna_cmd_stream_perf(stream, &p);
      offset = MAX2(offset, source->offset);
   }

   if (flags == ETNA_PM_PROCESS_PRE)
      pq->offset = offset;
   else
      pq->offset = 0;
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

   list_for_each_entry(struct etna_pm_source, source, &pq->signals, node)
      FREE(source);

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
   pq->offset = 0;

   etna_pm_query_get(ctx->stream, q, ETNA_PM_PROCESS_PRE);

   return true;
}

static void
etna_pm_end_query(struct etna_context *ctx, struct etna_query *q)
{
   q->active = false;

   etna_pm_query_get(ctx->stream, q, ETNA_PM_PROCESS_POST);
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

   result->u32 = pq->data[2] - pq->data[1];

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
   struct etna_pm_query *pq;
   struct etna_query *q;

    cfg = etna_pm_query_config(query_type);
    if (!cfg)
        return NULL;

   if (!etna_pm_cfg_supported(perfmon, cfg))
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

   list_inithead(&pq->signals);

   if (!etna_pm_add_signals(pq, perfmon, cfg)) {
       etna_pm_destroy_query(ctx, q);
       return NULL;
   }

   return q;
}

int
etna_pm_get_driver_query_info(struct pipe_screen *pscreen, unsigned index,
                              struct pipe_driver_query_info *info)
{
   if (!info)
      return ARRAY_SIZE(query_config);

   if (index >= ARRAY_SIZE(query_config))
      return 0;

   info->name = query_config[index].name;
   info->query_type = query_config[index].type;
   info->group_id = 0;

   return 1;
}
