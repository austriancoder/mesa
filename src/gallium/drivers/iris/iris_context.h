/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef IRIS_CONTEXT_H
#define IRIS_CONTEXT_H

#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_debug.h"
#include "intel/blorp/blorp.h"
#include "intel/common/gen_debug.h"
#include "intel/compiler/brw_compiler.h"
#include "iris_batch.h"
#include "iris_resource.h"
#include "iris_screen.h"

struct iris_bo;
struct iris_context;
struct blorp_batch;
struct blorp_params;

#define IRIS_MAX_TEXTURE_SAMPLERS 32
/* IRIS_MAX_ABOS and IRIS_MAX_SSBOS must be the same. */
#define IRIS_MAX_ABOS 16
#define IRIS_MAX_SSBOS 16
#define IRIS_MAX_VIEWPORTS 16

/**
 * Dirty flags.  When state changes, we flag some combination of these
 * to indicate that particular GPU commands need to be re-emitted.
 *
 * Each bit typically corresponds to a single 3DSTATE_* command packet, but
 * in rare cases they map to a group of related packets that need to be
 * emitted together.
 *
 * See iris_upload_render_state().
 */
#define IRIS_DIRTY_COLOR_CALC_STATE         (1ull <<  0)
#define IRIS_DIRTY_POLYGON_STIPPLE          (1ull <<  1)
#define IRIS_DIRTY_SCISSOR_RECT             (1ull <<  2)
#define IRIS_DIRTY_WM_DEPTH_STENCIL         (1ull <<  3)
#define IRIS_DIRTY_CC_VIEWPORT              (1ull <<  4)
#define IRIS_DIRTY_SF_CL_VIEWPORT           (1ull <<  5)
#define IRIS_DIRTY_PS_BLEND                 (1ull <<  6)
#define IRIS_DIRTY_BLEND_STATE              (1ull <<  7)
#define IRIS_DIRTY_RASTER                   (1ull <<  8)
#define IRIS_DIRTY_CLIP                     (1ull <<  9)
#define IRIS_DIRTY_SBE                      (1ull << 10)
#define IRIS_DIRTY_LINE_STIPPLE             (1ull << 11)
#define IRIS_DIRTY_VERTEX_ELEMENTS          (1ull << 12)
#define IRIS_DIRTY_MULTISAMPLE              (1ull << 13)
#define IRIS_DIRTY_VERTEX_BUFFERS           (1ull << 14)
#define IRIS_DIRTY_SAMPLE_MASK              (1ull << 15)
#define IRIS_DIRTY_SAMPLER_STATES_VS        (1ull << 16)
#define IRIS_DIRTY_SAMPLER_STATES_TCS       (1ull << 17)
#define IRIS_DIRTY_SAMPLER_STATES_TES       (1ull << 18)
#define IRIS_DIRTY_SAMPLER_STATES_GS        (1ull << 19)
#define IRIS_DIRTY_SAMPLER_STATES_PS        (1ull << 20)
#define IRIS_DIRTY_SAMPLER_STATES_CS        (1ull << 21)
#define IRIS_DIRTY_UNCOMPILED_VS            (1ull << 22)
#define IRIS_DIRTY_UNCOMPILED_TCS           (1ull << 23)
#define IRIS_DIRTY_UNCOMPILED_TES           (1ull << 24)
#define IRIS_DIRTY_UNCOMPILED_GS            (1ull << 25)
#define IRIS_DIRTY_UNCOMPILED_FS            (1ull << 26)
#define IRIS_DIRTY_UNCOMPILED_CS            (1ull << 27)
#define IRIS_DIRTY_VS                       (1ull << 28)
#define IRIS_DIRTY_TCS                      (1ull << 29)
#define IRIS_DIRTY_TES                      (1ull << 30)
#define IRIS_DIRTY_GS                       (1ull << 31)
#define IRIS_DIRTY_FS                       (1ull << 32)
#define IRIS_DIRTY_CS                       (1ull << 33)
#define IRIS_DIRTY_URB                      (1ull << 34)
#define IRIS_DIRTY_CONSTANTS_VS             (1ull << 35)
#define IRIS_DIRTY_CONSTANTS_TCS            (1ull << 36)
#define IRIS_DIRTY_CONSTANTS_TES            (1ull << 37)
#define IRIS_DIRTY_CONSTANTS_GS             (1ull << 38)
#define IRIS_DIRTY_CONSTANTS_FS             (1ull << 39)
#define IRIS_DIRTY_DEPTH_BUFFER             (1ull << 40)
#define IRIS_DIRTY_WM                       (1ull << 41)
#define IRIS_DIRTY_BINDINGS_VS              (1ull << 42)
#define IRIS_DIRTY_BINDINGS_TCS             (1ull << 43)
#define IRIS_DIRTY_BINDINGS_TES             (1ull << 44)
#define IRIS_DIRTY_BINDINGS_GS              (1ull << 45)
#define IRIS_DIRTY_BINDINGS_FS              (1ull << 46)
#define IRIS_DIRTY_BINDINGS_CS              (1ull << 47)
#define IRIS_DIRTY_SO_BUFFERS               (1ull << 48)
#define IRIS_DIRTY_SO_DECL_LIST             (1ull << 49)
#define IRIS_DIRTY_STREAMOUT                (1ull << 50)
#define IRIS_DIRTY_VF_SGVS                  (1ull << 51)

/**
 * Non-orthogonal state (NOS) dependency flags.
 *
 * Shader programs may depend on non-orthogonal state.  These flags are
 * used to indicate that a shader's key depends on the state provided by
 * a certain Gallium CSO.  Changing any CSOs marked as a dependency will
 * cause the driver to re-compute the shader key, possibly triggering a
 * shader recompile.
 */
enum iris_nos_dep {
   IRIS_NOS_FRAMEBUFFER,
   IRIS_NOS_DEPTH_STENCIL_ALPHA,
   IRIS_NOS_RASTERIZER,
   IRIS_NOS_BLEND,

   IRIS_NOS_COUNT,
};

struct iris_depth_stencil_alpha_state;

/**
 * Cache IDs for the in-memory program cache (ice->shaders.cache).
 */
enum iris_program_cache_id {
   IRIS_CACHE_VS  = MESA_SHADER_VERTEX,
   IRIS_CACHE_TCS = MESA_SHADER_TESS_CTRL,
   IRIS_CACHE_TES = MESA_SHADER_TESS_EVAL,
   IRIS_CACHE_GS  = MESA_SHADER_GEOMETRY,
   IRIS_CACHE_FS  = MESA_SHADER_FRAGMENT,
   IRIS_CACHE_CS  = MESA_SHADER_COMPUTE,
   IRIS_CACHE_BLORP,
};

/** @{
 *
 * Defines for PIPE_CONTROL operations, which trigger cache flushes,
 * synchronization, pipelined memory writes, and so on.
 *
 * The bits here are not the actual hardware values.  The actual fields
 * move between various generations, so we just have flags for each
 * potential operation, and use genxml to encode the actual packet.
 */
enum pipe_control_flags
{
   PIPE_CONTROL_FLUSH_LLC                       = (1 << 1),
   PIPE_CONTROL_LRI_POST_SYNC_OP                = (1 << 2),
   PIPE_CONTROL_STORE_DATA_INDEX                = (1 << 3),
   PIPE_CONTROL_CS_STALL                        = (1 << 4),
   PIPE_CONTROL_GLOBAL_SNAPSHOT_COUNT_RESET     = (1 << 5),
   PIPE_CONTROL_SYNC_GFDT                       = (1 << 6),
   PIPE_CONTROL_TLB_INVALIDATE                  = (1 << 7),
   PIPE_CONTROL_MEDIA_STATE_CLEAR               = (1 << 8),
   PIPE_CONTROL_WRITE_IMMEDIATE                 = (1 << 9),
   PIPE_CONTROL_WRITE_DEPTH_COUNT               = (1 << 10),
   PIPE_CONTROL_WRITE_TIMESTAMP                 = (1 << 11),
   PIPE_CONTROL_DEPTH_STALL                     = (1 << 12),
   PIPE_CONTROL_RENDER_TARGET_FLUSH             = (1 << 13),
   PIPE_CONTROL_INSTRUCTION_INVALIDATE          = (1 << 14),
   PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE        = (1 << 15),
   PIPE_CONTROL_INDIRECT_STATE_POINTERS_DISABLE = (1 << 16),
   PIPE_CONTROL_NOTIFY_ENABLE                   = (1 << 17),
   PIPE_CONTROL_FLUSH_ENABLE                    = (1 << 18),
   PIPE_CONTROL_DATA_CACHE_FLUSH                = (1 << 19),
   PIPE_CONTROL_VF_CACHE_INVALIDATE             = (1 << 20),
   PIPE_CONTROL_CONST_CACHE_INVALIDATE          = (1 << 21),
   PIPE_CONTROL_STATE_CACHE_INVALIDATE          = (1 << 22),
   PIPE_CONTROL_STALL_AT_SCOREBOARD             = (1 << 23),
   PIPE_CONTROL_DEPTH_CACHE_FLUSH               = (1 << 24),
};

#define PIPE_CONTROL_CACHE_FLUSH_BITS \
   (PIPE_CONTROL_DEPTH_CACHE_FLUSH |  \
    PIPE_CONTROL_DATA_CACHE_FLUSH |   \
    PIPE_CONTROL_RENDER_TARGET_FLUSH)

#define PIPE_CONTROL_CACHE_INVALIDATE_BITS  \
   (PIPE_CONTROL_STATE_CACHE_INVALIDATE |   \
    PIPE_CONTROL_CONST_CACHE_INVALIDATE |   \
    PIPE_CONTROL_VF_CACHE_INVALIDATE |      \
    PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE | \
    PIPE_CONTROL_INSTRUCTION_INVALIDATE)

/** @} */

/**
 * A compiled shader variant, containing a pointer to the GPU assembly,
 * as well as program data and other packets needed by state upload.
 *
 * There can be several iris_compiled_shader variants per API-level shader
 * (iris_uncompiled_shader), due to state-based recompiles (brw_*_prog_key).
 */
struct iris_compiled_shader {
   /** Reference to the uploaded assembly. */
   struct iris_state_ref assembly;

   /** Pointer to the assembly in the BO's map. */
   void *map;

   /** The program data (owned by the program cache hash table) */
   struct brw_stage_prog_data *prog_data;

   /**
    * Derived 3DSTATE_STREAMOUT and 3DSTATE_SO_DECL_LIST packets
    * (the VUE-based information for transform feedback outputs).
    */
   uint32_t *streamout;

   /**
    * Shader packets and other data derived from prog_data.  These must be
    * completely determined from prog_data.
    */
   uint8_t derived_data[0];
};

/**
 * Constant buffer (UBO) information.  See iris_set_const_buffer().
 */
struct iris_const_buffer {
   /** The resource and offset for the actual constant data */
   struct iris_state_ref data;

   /** The resource and offset for the SURFACE_STATE for pull access. */
   struct iris_state_ref surface_state;
};

/**
 * API context state that is replicated per shader stage.
 */
struct iris_shader_state {
   struct iris_const_buffer constbuf[PIPE_MAX_CONSTANT_BUFFERS];
   struct pipe_resource *ssbo[PIPE_MAX_SHADER_BUFFERS];
   struct iris_state_ref ssbo_surface_state[PIPE_MAX_SHADER_BUFFERS];
};

/**
 * Virtual table for generation-specific (genxml) function calls.
 */
struct iris_vtable {
   void (*destroy_state)(struct iris_context *ice);
   void (*init_render_context)(struct iris_screen *screen,
                               struct iris_batch *batch,
                               struct iris_vtable *vtbl,
                               struct pipe_debug_callback *dbg);
   void (*upload_render_state)(struct iris_context *ice,
                               struct iris_batch *batch,
                               const struct pipe_draw_info *draw);
   void (*emit_raw_pipe_control)(struct iris_batch *batch, uint32_t flags,
                                 struct iris_bo *bo, uint32_t offset,
                                 uint64_t imm);

   unsigned (*derived_program_state_size)(enum iris_program_cache_id id);
   void (*store_derived_program_state)(const struct gen_device_info *devinfo,
                                       enum iris_program_cache_id cache_id,
                                       struct iris_compiled_shader *shader);
   uint32_t *(*create_so_decl_list)(const struct pipe_stream_output_info *sol,
                                    const struct brw_vue_map *vue_map);
   void (*populate_vs_key)(const struct iris_context *ice,
                           struct brw_vs_prog_key *key);
   void (*populate_tcs_key)(const struct iris_context *ice,
                            struct brw_tcs_prog_key *key);
   void (*populate_tes_key)(const struct iris_context *ice,
                            struct brw_tes_prog_key *key);
   void (*populate_gs_key)(const struct iris_context *ice,
                           struct brw_gs_prog_key *key);
   void (*populate_fs_key)(const struct iris_context *ice,
                           struct brw_wm_prog_key *key);
};

/**
 * A pool containing SAMPLER_BORDER_COLOR_STATE entries.
 *
 * See iris_border_color.c for more information.
 */
struct iris_border_color_pool {
   struct iris_bo *bo;
   void *map;
   unsigned insert_point;

   /** Map from border colors to offsets in the buffer. */
   struct hash_table *ht;
};

/**
 * The API context (derived from pipe_context).
 *
 * Most driver state is tracked here.
 */
struct iris_context {
   struct pipe_context ctx;

   /** A debug callback for KHR_debug output. */
   struct pipe_debug_callback dbg;

   /** Slab allocator for iris_transfer_map objects. */
   struct slab_child_pool transfer_pool;

   struct iris_vtable vtbl;

   struct blorp_context blorp;

   /** The main batch for rendering. */
   struct iris_batch render_batch;

   struct {
      struct iris_uncompiled_shader *uncompiled[MESA_SHADER_STAGES];
      struct iris_compiled_shader *prog[MESA_SHADER_STAGES];
      struct brw_vue_map *last_vue_map;

      struct iris_shader_state state[MESA_SHADER_STAGES];

      struct u_upload_mgr *uploader;
      struct hash_table *cache;

      unsigned urb_size;
   } shaders;

   struct {
      uint64_t dirty;
      uint64_t dirty_for_nos[IRIS_NOS_COUNT];

      unsigned num_viewports;
      unsigned sample_mask;
      struct iris_blend_state *cso_blend;
      struct iris_rasterizer_state *cso_rast;
      struct iris_depth_stencil_alpha_state *cso_zsa;
      struct iris_vertex_element_state *cso_vertex_elements;
      struct pipe_blend_color blend_color;
      struct pipe_poly_stipple poly_stipple;
      struct pipe_viewport_state viewports[IRIS_MAX_VIEWPORTS];
      struct pipe_scissor_state scissors[IRIS_MAX_VIEWPORTS];
      struct pipe_stencil_ref stencil_ref;
      struct pipe_framebuffer_state framebuffer;

      /** GenX-specific current state */
      struct iris_genx_state *genx;

      struct iris_state_ref sampler_table[MESA_SHADER_STAGES];
      bool need_border_colors;
      struct iris_sampler_state *samplers[MESA_SHADER_STAGES][IRIS_MAX_TEXTURE_SAMPLERS];
      struct iris_sampler_view *textures[MESA_SHADER_STAGES][IRIS_MAX_TEXTURE_SAMPLERS];
      unsigned num_samplers[MESA_SHADER_STAGES];
      unsigned num_textures[MESA_SHADER_STAGES];

      struct pipe_stream_output_target *so_target[PIPE_MAX_SO_BUFFERS];
      bool streamout_active;
      /** 3DSTATE_STREAMOUT and 3DSTATE_SO_DECL_LIST packets */
      uint32_t *streamout;

      /** The SURFACE_STATE for a 1x1x1 null surface. */
      struct iris_state_ref unbound_tex;

      /** The SURFACE_STATE for a framebuffer-sized null surface. */
      struct iris_state_ref null_fb;

      struct u_upload_mgr *surface_uploader;
      // XXX: may want a separate uploader for "hey I made a CSO!" vs
      // "I'm streaming this out at draw time and never want it again!"
      struct u_upload_mgr *dynamic_uploader;

      struct iris_border_color_pool border_color_pool;

      /**
       * Resources containing streamed state which our render context
       * currently points to.  Used to re-add these to the validation
       * list when we start a new batch and haven't resubmitted commands.
       */
      struct {
         struct pipe_resource *cc_vp;
         struct pipe_resource *sf_cl_vp;
         struct pipe_resource *color_calc;
         struct pipe_resource *scissor;
         struct pipe_resource *blend;
      } last_res;
   } state;
};

#define perf_debug(dbg, ...) do {                      \
   if (INTEL_DEBUG & DEBUG_PERF)                       \
      dbg_printf(__VA_ARGS__);                         \
   if (unlikely(dbg))                                  \
      pipe_debug_message(dbg, PERF_INFO, __VA_ARGS__); \
} while(0)

double get_time(void);

struct pipe_context *
iris_create_context(struct pipe_screen *screen, void *priv, unsigned flags);

void iris_init_blit_functions(struct pipe_context *ctx);
void iris_init_clear_functions(struct pipe_context *ctx);
void iris_init_program_functions(struct pipe_context *ctx);
void iris_init_resource_functions(struct pipe_context *ctx);
void iris_init_query_functions(struct pipe_context *ctx);
void iris_update_compiled_shaders(struct iris_context *ice);

/* iris_blit.c */
void iris_blorp_surf_for_resource(struct blorp_surf *surf,
                                  struct pipe_resource *p_res,
                                  enum isl_aux_usage aux_usage,
                                  bool is_render_target);

/* iris_draw.c */

void iris_draw_vbo(struct pipe_context *ctx, const struct pipe_draw_info *info);

/* iris_pipe_control.c */

void iris_emit_pipe_control_flush(struct iris_batch *batch,
                                  uint32_t flags);
void iris_emit_pipe_control_write(struct iris_batch *batch, uint32_t flags,
                                  struct iris_bo *bo, uint32_t offset,
                                  uint64_t imm);
void iris_emit_end_of_pipe_sync(struct iris_batch *batch,
                                uint32_t flags);

void iris_cache_sets_clear(struct iris_batch *batch);
void iris_cache_flush_for_read(struct iris_batch *batch, struct iris_bo *bo);
void iris_cache_flush_for_render(struct iris_batch *batch,
                                 struct iris_bo *bo,
                                 enum isl_format format,
                                 enum isl_aux_usage aux_usage);
void iris_render_cache_add_bo(struct iris_batch *batch,
                              struct iris_bo *bo,
                              enum isl_format format,
                              enum isl_aux_usage aux_usage);
void iris_cache_flush_for_depth(struct iris_batch *batch, struct iris_bo *bo);
void iris_depth_cache_add_bo(struct iris_batch *batch, struct iris_bo *bo);

void iris_init_flush_functions(struct pipe_context *ctx);

/* iris_blorp.c */

void gen9_init_blorp(struct iris_context *ice);
void gen10_init_blorp(struct iris_context *ice);

/* iris_border_color.c */

void iris_init_border_color_pool(struct iris_context *ice);
void iris_border_color_pool_reserve(struct iris_context *ice, unsigned count);
uint32_t iris_upload_border_color(struct iris_context *ice,
                                  union pipe_color_union *color);

/* iris_state.c */

void gen9_init_state(struct iris_context *ice);
void gen10_init_state(struct iris_context *ice);

/* iris_program.c */
const struct shader_info *iris_get_shader_info(const struct iris_context *ice,
                                               gl_shader_stage stage);

/* iris_program_cache.c */

void iris_init_program_cache(struct iris_context *ice);
void iris_destroy_program_cache(struct iris_context *ice);
void iris_print_program_cache(struct iris_context *ice);
bool iris_bind_cached_shader(struct iris_context *ice,
                             enum iris_program_cache_id cache_id,
                             const void *key);
void iris_unbind_shader(struct iris_context *ice,
                        enum iris_program_cache_id cache_id);
void iris_upload_and_bind_shader(struct iris_context *ice,
                                 enum iris_program_cache_id cache_id,
                                 const void *key,
                                 const void *assembly,
                                 struct brw_stage_prog_data *prog_data,
                                 uint32_t *streamout);
const void *iris_find_previous_compile(const struct iris_context *ice,
                                       enum iris_program_cache_id cache_id,
                                       unsigned program_string_id);
bool iris_blorp_lookup_shader(struct blorp_batch *blorp_batch,
                              const void *key,
                              uint32_t key_size,
                              uint32_t *kernel_out,
                              void *prog_data_out);
bool iris_blorp_upload_shader(struct blorp_batch *blorp_batch,
                              const void *key, uint32_t key_size,
                              const void *kernel, uint32_t kernel_size,
                              const struct brw_stage_prog_data *prog_data,
                              uint32_t prog_data_size,
                              uint32_t *kernel_out,
                              void *prog_data_out);

#endif