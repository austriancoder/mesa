/*
 * Copyright (c) 2018 Etnaviv Project
 * Copyright (C) 2018 Zodiac Inflight Innovations
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

#include "compiler/nir/nir.h"
#include "eir.h"
#include "eir_compiler.h"
#include "eir_nir.h"
#include "eir_shader.h"
#include "gc/gc_instr.h"
#include "util/u_debug.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/ralloc.h"

struct eir_context
{
   struct eir_compiler *compiler;
   struct nir_shader *s;
   struct eir *ir;
   struct eir_shader_variant *variant;
   struct eir_block *block;
   bool error;

   unsigned uniforms;
   struct eir_register *nir_locals;
   struct eir_register *nir_ssa_values;
};

#define DBG(compiler, fmt, ...)                                   \
   do {                                                           \
         debug_printf("%s:%d: " fmt "\n", __FUNCTION__, __LINE__, \
                      ##__VA_ARGS__);                             \
   } while (0)

static struct eir_context *
compile_init(struct eir_compiler *compiler,
             struct eir_shader_variant *v)
{
   struct eir_context *ctx = rzalloc(NULL, struct eir_context);

   ctx->compiler = compiler;

   nir_shader *s = nir_shader_clone(ctx, v->shader->nir);
   ctx->ir = eir_create();
   ctx->ir->uniform_offset = s->num_uniforms * 4;
   ctx->s = s;
   ctx->variant = v;

   /* reset to sane values */
   v->fs_color_out_reg = -1;
   v->fs_depth_out_reg = -1;
   v->vs_pointsize_out_reg = -1;
   v->vs_pos_out_reg = -1;

   ctx->block = eir_block_create(ctx->ir);

   return ctx;
}

static void
compile_free(struct eir_context *ctx)
{
   ralloc_free(ctx);
}

static void
compile_error(struct eir_context *ctx, const char *format, ...)
{
   va_list ap;

   va_start(ap, format);
   _debug_vprintf(format, ap);
   va_end(ap);

   nir_print_shader(ctx->s, stdout);

   ctx->error = true;
   debug_assert(0);
}

#define compile_assert(ctx, cond) do { \
      if (!(cond)) compile_error((ctx), "failed assert: "#cond"\n"); \
   } while (0)

static struct eir_register
get_nir_dest(struct eir_context *ctx, const nir_dest *dest)
{
   if (dest->is_ssa) {
      struct eir_register dst = eir_temp_register(ctx->ir, dest->ssa.num_components);

      ctx->nir_ssa_values[dest->ssa.index] = dst;
      return dst;
   } else {
      return ctx->nir_locals[dest->reg.reg->index];
   }
}

/**
 * Construct an identity swizzle for the set of enabled channels given by \p
 * mask. The result will only reference channels enabled in the provided \p
 * mask, assuming that \p mask is non-zero.
 */
static inline unsigned
eir_swizzle_for_mask(unsigned mask)
{
   unsigned last = (mask ? ffs(mask) - 1 : 0);
   unsigned swz[4];

   for (unsigned i = 0; i < 4; i++)
      last = swz[i] = (mask & (1 << i) ? i : last);

   return INST_SWIZ(swz[0], swz[1], swz[2], swz[3]);
}

/**
 * Construct an identity swizzle for the first \p n components of a vector.
 */
static inline unsigned
eir_swizzle_for_size(unsigned n)
{
   return eir_swizzle_for_mask((1 << n) - 1);
}

static struct eir_register
get_nir_src(const struct eir_context *ctx, const nir_src *src,
            unsigned num_components)
{
   nir_const_value *const_value = nir_src_as_const_value(*src);
   struct eir_register reg;

   if (const_value) {
      assert(src->is_ssa);
      uint32_t c[src->ssa->num_components];

      nir_const_value_to_array(c, const_value, src->ssa->num_components, u32);

      return eir_uniform_register_vec4_ui(ctx->ir, src->ssa->num_components, c);
   }

   if (src->is_ssa)
      reg = ctx->nir_ssa_values[src->ssa->index];
   else
      reg = ctx->nir_locals[src->reg.reg->index];

   reg.swizzle = eir_swizzle_for_size(num_components);

   return reg;
}

static inline unsigned
eir_writemask_for_size(unsigned n)
{
   return (1 << n) - 1;
}

static inline unsigned
eir_swizzle_for_nir_swizzle(const uint8_t swizzle[4])
{
   return INST_SWIZ(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
}

static inline void
nir_alu_src_to_eir(const struct eir_context *ctx, const nir_alu_src *src, struct eir_register *reg)
{
   *reg = get_nir_src(ctx, &src->src, 4);
   reg->swizzle = eir_swizzle_for_nir_swizzle(src->swizzle);
   reg->abs = src->abs;
   reg->neg = src->negate;
}

static void
emit_alu(struct eir_context *ctx, nir_alu_instr *alu)
{
   const nir_op_info *info = &nir_op_infos[alu->op];

   struct eir_instruction *instr = NULL;
   struct eir_register src[3] = { };

   struct eir_register dst;
   dst = get_nir_dest(ctx, &alu->dest.dest);
   dst.writemask = alu->dest.write_mask;

   for (unsigned i = 0; i < info->num_inputs; i++)
      nir_alu_src_to_eir(ctx, &alu->src[i], &src[i]);

   switch (alu->op) {
   case nir_op_f2i32:
      /* nothing to do - first seen when TGSI uses ARL opc */
      break;

   case nir_op_fmov:
      instr = eir_MOV(ctx->block, &dst, &src[0]);
      break;

   case nir_op_fadd:
      instr = eir_ADD(ctx->block, &dst, &src[0], &src[1]);
      break;

   case nir_op_fceil:
      instr = eir_CEIL(ctx->block, &dst, &src[0]);
      break;

   case nir_op_fddx:
      instr = eir_DSX(ctx->block, &dst, &src[0]);
      break;

   case nir_op_fddy:
      instr = eir_DSY(ctx->block, &dst, &src[0]);
      break;

   case nir_op_fdot2:
      /* fall-through */
   case nir_op_fdot3:
      /* fall-through */
   case nir_op_fdot4:
      unreachable("Should be lowered by fdot_replicates = true");
      break;

   case nir_op_fdot_replicated2:
      instr = eir_DP2(ctx->block, &dst, &src[0], &src[1]);
      break;

   case nir_op_fdot_replicated3:
      instr = eir_DP3(ctx->block, &dst, &src[0], &src[1]);
      break;

   case nir_op_fdot_replicated4:
      instr = eir_DP4(ctx->block, &dst, &src[0], &src[1]);
      break;

   case nir_op_fexp2:
      instr = eir_EXP(ctx->block, &dst, &src[0]);
      break;

   case nir_op_ffloor:
      instr = eir_FLOOR(ctx->block, &dst, &src[0]);
      break;

   case nir_op_ffma:
      instr = eir_MAD(ctx->block, &dst, &src[0], &src[1], &src[2]);
      break;

   case nir_op_ffract:
      instr = eir_FRC(ctx->block, &dst, &src[0]);
      break;

   case nir_op_flog2:
      /* TODO: new vs. old hw */
      instr = eir_LOG(ctx->block, &dst, &src[0]);
      break;

   case nir_op_flrp:
      unreachable("not reached: should be lowered by lower_flrp32 = true");
      break;

   case nir_op_fmax:
      instr = eir_SELECT(ctx->block, &dst, &src[0], &src[1], &src[0]);
      instr->gc.condition = GC_COND_LT;
      break;

   case nir_op_fmin:
      instr = eir_SELECT(ctx->block, &dst, &src[0], &src[1], &src[2]);
      instr->gc.condition = GC_COND_GT;
      break;

   case nir_op_fmul:
      instr = eir_MUL(ctx->block, &dst, &src[0], &src[1]);
      break;

   case nir_op_fabs:
      /* fall-through */
   case nir_op_fneg:
      /* fall-through */
   case nir_op_fsat:
      unreachable("not reached: should be lowered by lower_source mods");
      break;

   case nir_op_fpow:
      unreachable("not reached: should be lowered by lower_fpow = true");
      break;

   case nir_op_frcp:
      instr = eir_RCP(ctx->block, &dst, &src[0]);
      break;

   case nir_op_frsq:
      instr = eir_RSQ(ctx->block, &dst, &src[0]);
      break;

   case nir_op_fcsel:
      instr = eir_SELECT(ctx->block, &dst, &src[0], &src[1], &src[2]);
      instr->gc.condition = GC_COND_NZ;
      break;

   case nir_op_fcos:
   case nir_op_fsin: {
      struct eir_register tmp = eir_temp_register(ctx->ir, 4);
      struct eir_register m_2_pi = eir_uniform_register_f(ctx->ir, M_2_PI);

      eir_MUL(ctx->block, &tmp, &src[0], &m_2_pi);

      if (alu->op == nir_op_fsin)
         instr = eir_SIN(ctx->block, &dst, &tmp);
      else
         instr = eir_COS(ctx->block, &dst, &tmp);
      }
      break;

   case nir_op_fsqrt:
      instr = eir_SQRT(ctx->block, &dst, &src[0]);
      break;

   case nir_op_ftrunc:
      unreachable("not reached: should be lowered by lower_ftrunc = true");
      break;

   case nir_op_i2f32:
   case nir_op_u2f32:
      instr = eir_MOV(ctx->block, &dst, &src[0]);
      break;

   case nir_op_seq:
      instr = eir_SET(ctx->block, &dst, &src[0], GC_COND_EQ, &src[1]);
      break;

   case nir_op_sge:
      instr = eir_SET(ctx->block, &dst, &src[0], GC_COND_GE, &src[1]);
      break;

   case nir_op_slt:
      instr = eir_SET(ctx->block, &dst, &src[0], GC_COND_LT, &src[1]);
      break;

   case nir_op_sne:
      instr = eir_SET(ctx->block, &dst, &src[0], GC_COND_NE, &src[1]);
      break;

   case nir_op_b2f32:
      /* fall-through */
   case nir_op_b2i32:
      /* fall-through */
   case nir_op_f2b1:
      /* fall-through */
   case nir_op_i2b1:
      /* fall-through */
   case nir_op_flt:
      /* fall-through */
   case nir_op_fge:
      /* fall-through */
   case nir_op_feq:
      /* fall-through */
   case nir_op_fne:
      /* fall-through */
   case nir_op_ilt:
      /* fall-through */
   case nir_op_ige:
      /* fall-through */
   case nir_op_ieq:
      /* fall-through */
   case nir_op_ine:
      /* fall-through */
   case nir_op_ult:
      /* fall-through */
   case nir_op_uge:
      /* fall-through */
   case nir_op_ball_fequal2:
      /* fall-through */
   case nir_op_ball_fequal3:
      /* fall-through */
   case nir_op_ball_fequal4:
      /* fall-through */
   case nir_op_bany_fnequal2:
      /* fall-through */
   case nir_op_bany_fnequal3:
      /* fall-through */
   case nir_op_bany_fnequal4:
      /* fall-through */
   case nir_op_ball_iequal2:
      /* fall-through */
   case nir_op_ball_iequal3:
      /* fall-through */
   case nir_op_ball_iequal4:
      /* fall-through */
   case nir_op_bany_inequal2:
      /* fall-through */
   case nir_op_bany_inequal3:
      /* fall-through */
   case nir_op_bany_inequal4:
      /* fall-through */
   case nir_op_bcsel:
      /* fall-through */
   case nir_op_imov:
      /* fall-through */
   case nir_op_iand:
      /* fall-through */
   case nir_op_ixor:
      /* fall-through */
   case nir_op_ior:
      /* fall-through */
   case nir_op_inot:
      unreachable("not reached: should be lowered by nir_lower_bool_to_float");
      break;

   default:
      compile_error(ctx, "Unhandled ALU op: %s\n", info->name);
      break;
   }

   if (instr)
      instr->gc.saturate = alu->dest.saturate;
}

static void
emit_intrinsic(struct eir_context *ctx, nir_intrinsic_instr *instr)
{
   const nir_intrinsic_info *info = &nir_intrinsic_infos[instr->intrinsic];

   switch (instr->intrinsic) {
   case nir_intrinsic_discard:
      eir_TEXKILL(ctx->block);
      break;

   case nir_intrinsic_load_input: {
      nir_const_value *const_offset = nir_src_as_const_value(instr->src[0]);
      assert(const_offset);

      struct eir_register src = {
         .index = instr->const_index[0] + const_offset->u32,
         .type = EIR_REG_TEMP,
      };

      ctx->nir_ssa_values[instr->dest.ssa.index] = src;
      break;
   }

   case nir_intrinsic_load_uniform: {
      if (nir_src_is_const(instr->src[0])) {
         const unsigned load_offset = nir_src_as_uint(instr->src[0]);

         /* Offsets are in bytes but they should always be multiples of 4 */
         assert(load_offset % 4 == 0);

         struct eir_register src = {
            .index = instr->const_index[0],
            .type = EIR_REG_UNIFORM,
         };

         ctx->nir_ssa_values[instr->dest.ssa.index] = src;
      } else {
         compile_error(ctx, "indirect load_uniform not supported yet\n");
      }

      break;
   }

   case nir_intrinsic_store_output: {
      /* no support for indirect outputs */
      nir_const_value *const_offset = nir_src_as_const_value(instr->src[1]);
      assert(const_offset);

      const int idx = nir_intrinsic_base(instr);
      struct eir_register src = get_nir_src(ctx, &instr->src[0], instr->num_components);

      struct eir_register dst = {
         .index = idx,
         .type = EIR_REG_TEMP,
         .writemask = eir_writemask_for_size(instr->num_components),
      };

      eir_MOV(ctx->block, &dst, &src);

      break;
   }

   case nir_intrinsic_nop:
      eir_NOP(ctx->block);
      break;

   default:
      compile_error(ctx, "Unhandled intrinsic type: %s\n", info->name);
      break;
   }
}

static void
emit_tex(struct eir_context *ctx, nir_tex_instr *tex)
{
   const nir_op_info *info = &nir_op_infos[tex->op];
   struct eir_register sampler;
   struct eir_register coordinate;
   struct eir_register bias;
   MAYBE_UNUSED struct eir_register lod;

   struct eir_register dst;
   dst = get_nir_dest(ctx, &tex->dest);
   dst.writemask = INST_COMPS_X | INST_COMPS_Y | INST_COMPS_Z | INST_COMPS_W;

   sampler.type = EIR_REG_SAMPLER;
   sampler.index = tex->texture_index;
   sampler.swizzle = INST_SWIZ_IDENTITY;
   /* TODO: amode */

   for (unsigned i = 0; i < tex->num_srcs; i++) {
      const unsigned src_size = nir_tex_instr_src_size(tex, i);

      switch (tex->src[i].src_type) {
      case nir_tex_src_coord:
         coordinate = get_nir_src(ctx, &tex->src[i].src, src_size);
         coordinate.swizzle = INST_SWIZ_IDENTITY;
         break;

      case nir_tex_src_projector:
         unreachable("Should be lowered by nir_lower_tex");
         break;

      case nir_tex_src_bias:
         bias = get_nir_src(ctx, &tex->src[i].src, src_size);
         break;

      case nir_tex_src_lod:
         lod = get_nir_src(ctx, &tex->src[i].src, src_size);
         break;

      default:
         compile_error(ctx, "Unhandled NIR tex src type: %d\n",
                       tex->src[i].src_type);
         return;
      }
   }

   switch (tex->op) {
   case nir_texop_tex:
      eir_TEXLD(ctx->block, &dst, &sampler, &coordinate);
      break;

   case nir_texop_txb:
      eir_TEXLDB(ctx->block, &dst, &sampler, &bias);
      break;

   case nir_texop_txl:
      eir_TEXLDL(ctx->block, &dst, &sampler, &coordinate);
      break;

   case nir_texop_txs: {
         const int dest_size = nir_tex_instr_dest_size(tex);
         const unsigned unit = tex->texture_index;

         assert(dest_size < 3);

         const uint32_t v[] = {
            unit,
            unit,
            unit,
         };

         const enum eir_uniform_contents c[] = {
            EIR_UNIFORM_IMAGE_WIDTH,
            EIR_UNIFORM_IMAGE_HEIGHT,
            EIR_UNIFORM_IMAGE_DEPTH,
         };

         /* we do not support lod yet */
         assert(nir_tex_instr_src_index(tex, nir_tex_src_lod) == 0);

         struct eir_register size = eir_uniform_register_vec4(ctx->ir, 3, c, v);

         /* TODO: get rid of mov and use uniform directly */
         eir_MOV(ctx->block, &dst, &size);
      }
      break;

   default:
      compile_error(ctx, "Unhandled NIR tex op: %d\n", info->name);
      break;
   }
}

static void
emit_jump(struct eir_context *ctx, nir_jump_instr *jump)
{
   compile_error(ctx, "Unhandled NIR jump type: %d\n", jump->type);
}

static void
emit_undef(struct eir_context *ctx, nir_ssa_undef_instr *undef)
{
   ctx->nir_ssa_values[undef->def.index] = eir_temp_register(ctx->ir, undef->def.num_components);
}

static void
emit_instr(struct eir_context *ctx, nir_instr *instr)
{
   switch (instr->type) {
   case nir_instr_type_alu:
      emit_alu(ctx, nir_instr_as_alu(instr));
      break;
   case nir_instr_type_intrinsic:
      emit_intrinsic(ctx, nir_instr_as_intrinsic(instr));
      break;
   case nir_instr_type_load_const:
      /* dealt with when using nir_src */
      break;
   case nir_instr_type_tex:
      emit_tex(ctx, nir_instr_as_tex(instr));
      break;
   case nir_instr_type_jump:
      emit_jump(ctx, nir_instr_as_jump(instr));
      break;
   case nir_instr_type_ssa_undef:
      emit_undef(ctx, nir_instr_as_ssa_undef(instr));
      break;
   case nir_instr_type_phi:
      /* we have converted phi webs to regs in NIR by now */
      compile_error(ctx, "Unexpected NIR instruction type: %d\n", instr->type);
      break;
   case nir_instr_type_deref:
   case nir_instr_type_call:
   case nir_instr_type_parallel_copy:
      compile_error(ctx, "Unhandled NIR instruction type: %d\n", instr->type);
      break;
   }
}

static void emit_cf_list(struct eir_context *ctx, struct exec_list *list);

static void
emit_block(struct eir_context *ctx, nir_block *nblock)
{
   nir_foreach_instr(instr, nblock) {
      emit_instr(ctx, instr);
      if (ctx->error)
         return;
   }
}

static void
emit_if(struct eir_context *ctx, nir_if *nif)
{
   nir_block *nir_else_block = nir_if_first_else_block(nif);
   bool empty_else_block = (nir_else_block == nir_if_last_else_block(nif) &&
                           exec_list_is_empty(&nir_else_block->instr_list));

   struct eir_block *then_block = eir_block_create(ctx->ir);
   struct eir_block *after_block = eir_block_create(ctx->ir);
   struct eir_block *else_block = NULL;

   if (empty_else_block)
      else_block = after_block;
   else
      else_block = eir_block_create(ctx->ir);

   eir_link_blocks(ctx->block, else_block);
   eir_link_blocks(ctx->block, then_block);

   assert(nif->condition.is_ssa);
   assert(nif->condition.ssa->parent_instr->type == nir_instr_type_alu);

   nir_alu_instr *parent_alu = nir_instr_as_alu(nif->condition.ssa->parent_instr);

   struct eir_register src0, src1;
   nir_alu_src_to_eir(ctx, &parent_alu->src[0], &src0);
   nir_alu_src_to_eir(ctx, &parent_alu->src[1], &src1);

   struct eir_instruction *instr = eir_BRANCH(ctx->block, &src0, &src1);

   switch (parent_alu->op) {
   case nir_op_feq:
   case nir_op_seq:
      instr->gc.condition = GC_COND_NE;
      break;

   case nir_op_fne:
   case nir_op_sne:
      instr->gc.condition = GC_COND_EQ;
      break;

   case nir_op_flt:
   case nir_op_slt:
      instr->gc.condition = GC_COND_GT;
      break;

   case nir_op_fge:
   case nir_op_sge:
      instr->gc.condition = GC_COND_LE;
      break;

   default:
      unreachable("not supported opc");
   }

   ctx->block = then_block;
   eir_link_blocks(ctx->block, after_block);
   emit_cf_list(ctx, &nif->then_list);

   if (!empty_else_block) {
      ctx->block = else_block;
      emit_cf_list(ctx, &nif->else_list);

      eir_link_blocks(ctx->block, after_block);
      eir_link_blocks(ctx->block, else_block);
   }

   ctx->block = after_block;
}

static void
emit_loop(struct eir_context *ctx, nir_loop *nloop)
{
   emit_cf_list(ctx, &nloop->body);

   ctx->variant->num_loops++;
}

static void
emit_cf_list(struct eir_context *ctx, struct exec_list *list)
{
   foreach_list_typed(nir_cf_node, node, node, list) {
      switch (node->type) {
      case nir_cf_node_block:
         emit_block(ctx, nir_cf_node_as_block(node));
         break;
      case nir_cf_node_if:
         emit_if(ctx, nir_cf_node_as_if(node));
         break;
      case nir_cf_node_loop:
         emit_loop(ctx, nir_cf_node_as_loop(node));
         break;
      case nir_cf_node_function:
         compile_error(ctx, "TODO\n");
         break;
      }
   }
}

static void
emit_function(struct eir_context *ctx, nir_function_impl *impl)
{
   emit_cf_list(ctx, &impl->body);
}

static void
setup_input(struct eir_context *ctx, nir_variable *in)
{
   unsigned array_len = MAX2(glsl_get_length(in->type), 1);
   unsigned ncomp = glsl_get_components(in->type);
   unsigned n = in->data.driver_location;
   unsigned slot = in->data.location;

   DBG(ctx->compiler, "; in: slot=%u, len=%ux%u, drvloc=%u",
         slot, array_len, ncomp, n);

   compile_assert(ctx, n < ARRAY_SIZE(ctx->ir->inputs));

   if (ctx->s->info.stage == MESA_SHADER_FRAGMENT) {
      eir_assign_input(ctx->ir, n, slot, ncomp);
   } else if (ctx->s->info.stage == MESA_SHADER_VERTEX) {
      eir_assign_input(ctx->ir, n, slot, ncomp);
   } else {
      compile_error(ctx, "unknown shader type: %d\n", ctx->s->info.stage);
   }
}

static void
setup_output(struct eir_context *ctx, nir_variable *out)
{
   unsigned array_len = MAX2(glsl_get_length(out->type), 1);
   unsigned ncomp = glsl_get_components(out->type);
   unsigned n = out->data.driver_location;
   unsigned slot = out->data.location;

   DBG(ctx->compiler, "; out: slot=%u, len=%ux%u, drvloc=%u",
         slot, array_len, ncomp, n);

   compile_assert(ctx, n < ARRAY_SIZE(ctx->ir->outputs));

   if (ctx->s->info.stage == MESA_SHADER_FRAGMENT) {
      switch (slot) {
      case FRAG_RESULT_DEPTH:
         eir_assign_output(ctx->ir, n, slot, ncomp);
         break;

      case FRAG_RESULT_COLOR:
         eir_assign_output(ctx->ir, n, slot, ncomp);
         break;

      default:
         if (slot >= FRAG_RESULT_DATA0) {
            eir_assign_output(ctx->ir, n, slot, ncomp);
            break;
         }

         compile_error(ctx, "unknown FS output name: %s\n",
                       gl_frag_result_name(slot));
      }
   } else if (ctx->s->info.stage == MESA_SHADER_VERTEX) {
      switch (slot) {
      case VARYING_SLOT_POS:
         eir_assign_output(ctx->ir, n, slot, ncomp);
         break;

      case VARYING_SLOT_PSIZ:
         eir_assign_output(ctx->ir, n, slot, ncomp);
         break;

      case VARYING_SLOT_COL0:
         eir_assign_output(ctx->ir, n, slot, ncomp);
         break;

      default:
         if (slot >= VARYING_SLOT_VAR0) {
            eir_assign_output(ctx->ir, n, slot, ncomp);
            break;
         }

         if ((VARYING_SLOT_TEX0 <= slot) && (slot <= VARYING_SLOT_TEX7)) {
            eir_assign_output(ctx->ir, n, slot, ncomp);
            break;
         }

         compile_error(ctx, "unknown VS output name: %s\n",
               gl_varying_slot_name(slot));
      }
   } else {
      compile_error(ctx, "unknown shader type: %d\n", ctx->s->info.stage);
   }
}

static int
max_drvloc(struct exec_list *vars)
{
   int drvloc = -1;

   nir_foreach_variable(var, vars)
      drvloc = MAX2(drvloc, (int)var->data.driver_location);

   return drvloc;
}

static void
emit_instructions(struct eir_context *ctx)
{
   const unsigned ninputs  = max_drvloc(&ctx->s->inputs) + 1;
   const unsigned noutputs = max_drvloc(&ctx->s->outputs) + 1;

   // keep t0..tn registers reserved for inputs and outputs
   const unsigned reserved = MAX2(ninputs, noutputs);
   for (unsigned i = 0; i < reserved; i++)
      eir_temp_register(ctx->ir, 4);

   nir_function_impl *fxn = nir_shader_get_entrypoint(ctx->s);

   ctx->nir_locals = ralloc_array(ctx, struct eir_register, fxn->reg_alloc);
   ctx->nir_ssa_values = ralloc_array(ctx, struct eir_register, fxn->ssa_alloc);

   foreach_list_typed(nir_register, reg, node, &fxn->registers) {
      assert(reg->num_array_elems == 0);
      assert(reg->bit_size == 32);

      ctx->nir_locals[reg->index] = eir_temp_register(ctx->ir, 4);
   }

   if (ctx->s->num_uniforms > 0)
      ctx->uniforms = ctx->s->num_uniforms / 16;

   nir_foreach_variable(var, &ctx->s->inputs)
      setup_input(ctx, var);

   nir_foreach_variable(var, &ctx->s->outputs)
      setup_output(ctx, var);

   emit_function(ctx, fxn);
}

static void
setup_special_vert_register(const struct eir *ir, unsigned i, struct eir_shader_variant *v)
{
   const int reg = ir->outputs[i].reg;

   switch (ir->outputs[i].slot) {
   case VARYING_SLOT_POS:
      v->vs_pos_out_reg = reg;
      break;
   default:
      /* nothing to do */
      break;
   }
}

static void
setup_special_frag_register(const struct eir *ir, unsigned i, struct eir_shader_variant *v)
{
   const int reg = ir->outputs[i].reg;

   switch (ir->outputs[i].slot) {
   case FRAG_RESULT_DEPTH:
      v->fs_depth_out_reg = reg;
      break;
   case FRAG_RESULT_COLOR:
      v->fs_color_out_reg = reg;
      break;
   default:
      /* nothing to do */
      break;
   }
}

static void
setup_shader_io(struct eir_context *ctx, struct eir_shader_variant *v)
{
   const struct eir *ir = ctx->ir;

   for (unsigned i = 0; i < ir->num_inputs; i++) {
      v->inputs[i].reg = ir->inputs[i].reg;
      v->inputs[i].slot = ir->inputs[i].slot;
      v->inputs[i].ncomp = ir->inputs[i].ncomp;

      /* TODO:
      v->inputs[n].interpolate = in->data.interpolation;
      */
   }

   for (unsigned i = 0; i < ir->num_outputs; i++) {
      v->outputs[i].reg = ir->outputs[i].reg;
      v->outputs[i].slot = ir->outputs[i].slot;
      v->outputs[i].ncomp = ir->outputs[i].ncomp;

      if (v->shader->type == MESA_SHADER_VERTEX)
         setup_special_vert_register(ir, i, v);
      else if (v->shader->type == MESA_SHADER_FRAGMENT)
         setup_special_frag_register(ir, i, v);
   }

   v->num_inputs = ir->num_inputs;
   v->num_outputs = ir->num_outputs;
}

int
eir_compile_shader_nir(struct eir_compiler *compiler,
                       struct eir_shader_variant *v)
{
   int ret = 0;
   struct eir_context *ctx;
   bool success;

   assert(!v->ir);
   assert(v->shader->mem_ctx);

   ctx = compile_init(compiler, v);
   if (!ctx) {
      DBG(compiler, "INIT failed!");
      ret = -1;
      goto out;
   }

   v->ir = ctx->ir;

   emit_instructions(ctx);

   if (ctx->error) {
      DBG(compiler, "EMIT failed!");
      ret = -1;
      goto out;
   }

   if (eir_compiler_debug & EIR_DBG_OPTMSGS) {
      printf("AFTER emitting:\n");
      eir_print(ctx->ir);
   }

   eir_optimize(ctx->ir);
   if (eir_compiler_debug & EIR_DBG_OPTMSGS) {
      printf("AFTER eir_optimize:\n");
      eir_print(ctx->ir);
   }

   eir_legalize(ctx->ir);
   if (eir_compiler_debug & EIR_DBG_OPTMSGS) {
      printf("AFTER legalization:\n");
      eir_print(ctx->ir);
   }

   eir_calculate_live_intervals(ctx->ir);
   if (eir_compiler_debug & EIR_DBG_OPTMSGS) {
      printf("AFTER live-ranges:\n");
      eir_print(ctx->ir);
   }

   success = eir_register_allocate(ctx->ir, v->shader->type, ctx->compiler);
   if (!success) {
      DBG(compiler, "RA failed!");
      ret = -1;
      goto out;
   }

   if (eir_compiler_debug & EIR_DBG_OPTMSGS) {
      printf("AFTER RA:\n");
      eir_print(ctx->ir);
   }

   setup_shader_io(ctx, v);

   v->num_temps = ctx->ir->num_temps;
   v->const_size = ctx->ir->uniform_offset;

   util_dynarray_clone(&v->uniforms, v->shader->mem_ctx, &ctx->ir->uniform_alloc);

out:
   if (ret) {
      if (v->ir)
         eir_destroy(v->ir);
      v->ir = NULL;
   }

   compile_free(ctx);

   return ret;
}
