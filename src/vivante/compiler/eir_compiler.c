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
   struct gc_instr_src *nir_ssa_values;
};

#define DBG(compiler, fmt, ...)                                   \
   do {                                                           \
      if (compiler->debug & EIR_DBG_MSGS)                         \
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
   ctx->s = eir_optimize_nir(v->shader, s);
   ctx->ir = eir_create();
   ctx->variant = v;

   /* reset to sane values */
   v->fs_color_out_reg = -1;
   v->fs_depth_out_reg = -1;
   v->vs_pointsize_out_reg = -1;
   v->vs_pos_out_reg = -1;

   ctx->block = eir_block_create(ctx->ir);

   /* TODO: uniforms handling */
   ctx->ir->const_size = align(ctx->s->num_uniforms, 4);

   return ctx;
}

static void
compile_free(struct eir_context *ctx)
{
   ralloc_free(ctx);
}

struct eir_compiler *
eir_compiler_init(struct eir_compiler_options *options)
{
   assert(options);
   struct eir_compiler *compiler = rzalloc(NULL, struct eir_compiler);

   if (!compiler)
      return NULL;

   compiler->options = *options;
   compiler->set = eir_ra_alloc_reg_set(compiler);

   return compiler;
}

void
eir_compiler_free(const struct eir_compiler *compiler)
{
   ralloc_free((void *)compiler);
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

static struct gc_instr_dst
get_nir_dest(struct eir_context *ctx, const nir_dest *dest)
{
   assert(!dest->is_ssa);

   struct gc_instr_dst d = {
      .reg = dest->reg.reg->index
   };

   return d;
}

static struct gc_instr_src
get_nir_src(struct eir_context *ctx, const nir_src *src)
{
   struct gc_instr_src s;

   if (src->is_ssa) {
      s = ctx->nir_ssa_values[src->ssa->index];
   } else {
      s.amode = GC_ADDRESSING_MODE_DIRECT;
      s.reg = src->reg.reg->index;
      s.rgroup = GC_REGISTER_GROUP_TEMP;
   }

   if (src->reg.reg->is_global)
      s.rgroup = GC_REGISTER_GROUP_UNIFORM;

   return s;
}

static inline unsigned
eir_swizzle_for_nir_swizzle(uint8_t swizzle[4])
{
   return INST_SWIZ(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
}

static void
emit_alu(struct eir_context *ctx, nir_alu_instr *alu)
{
   const nir_op_info *info = &nir_op_infos[alu->op];

   struct gc_instr *instr = NULL;
   struct gc_instr_src src[3] = { };

   struct gc_instr_dst dst;
   dst = get_nir_dest(ctx, &alu->dest.dest);
   dst.comps = alu->dest.write_mask;

   for (unsigned i = 0; i < info->num_inputs; i++) {
      src[i] = get_nir_src(ctx, &alu->src[i].src);
      src[i].swiz = eir_swizzle_for_nir_swizzle(alu->src[i].swizzle);
      src[i].abs = alu->src[i].abs;
      src[i].neg = alu->src[i].negate;
   }

   switch (alu->op) {
   case nir_op_fmov:
   case nir_op_imov:
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
      unreachable("Should be lowered by fdot_replicates = true");
      break;

   case nir_op_fdot_replicated2:
      instr = eir_DP2(ctx->block, &dst, &src[0], &src[1]);
      break;

   case nir_op_fdot_replicated3:
      instr = eir_DP3(ctx->block, &dst, &src[0], &src[1]);
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

   case nir_op_fmax:
      instr = eir_SELECT(ctx->block, &dst, &src[0], &src[1], &src[2]);
      instr->condition = GC_COND_LT;
      break;

   case nir_op_fmin:
      instr = eir_SELECT(ctx->block, &dst, &src[0], &src[1], &src[2]);
      instr->condition = GC_COND_GT;
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

   case nir_op_frcp:
      instr = eir_RCP(ctx->block, &dst, &src[0]);
      break;

   case nir_op_frsq:
      instr = eir_RSQ(ctx->block, &dst, &src[0]);
      break;

   case nir_op_fsqrt:
      instr = eir_SQRT(ctx->block, &dst, &src[0]);
      break;

   case nir_op_seq:
      /* fall-through */
   case nir_op_feq:
      instr = eir_SET(ctx->block, &dst, &src[0], &src[1]);
      instr->condition = GC_COND_EQ;
      break;

   case nir_op_sge:
      /* fall-through */
   case nir_op_fge:
      instr = eir_SET(ctx->block, &dst, &src[0], &src[1]);
      instr->condition = GC_COND_GE;
      break;

   case nir_op_slt:
      /* fall-through */
   case nir_op_flt:
      instr = eir_SET(ctx->block, &dst, &src[0], &src[1]);
      instr->condition = GC_COND_LT;
      break;

   case nir_op_sne:
      /* fall-through */
   case nir_op_fne:
      instr = eir_SET(ctx->block, &dst, &src[0], &src[1]);
      instr->condition = GC_COND_NE;
      break;

   default:
      compile_error(ctx, "Unhandled ALU op: %s\n", info->name);
      break;
   }

   if (instr)
      instr->saturate = alu->dest.saturate;
}

static void
emit_intrinsic(struct eir_context *ctx, nir_intrinsic_instr *instr)
{
   const nir_intrinsic_info *info = &nir_intrinsic_infos[instr->intrinsic];

   switch (instr->intrinsic) {
   case nir_intrinsic_nop:
      eir_NOP(ctx->block);
      break;

   case nir_intrinsic_load_uniform:
      unreachable("Should be lowered by eir_nir_lower_uniforms");
      break;

   default:
      //compile_error(ctx, "Unhandled intrinsic type: %s\n", info->name);
      break;
   }
}

static void
emit_load_const(struct eir_context *ctx, nir_load_const_instr *instr)
{
   struct gc_instr_src reg;

   if (instr->def.num_components == 4) {
      reg = eir_uniform_vec4_u32(ctx->ir, instr->value.u32);
   } else if (instr->def.num_components == 1) {
      reg = eir_uniform_u32(ctx->ir, instr->value.u32[0]);
   } else {
      unreachable("oh no..");
   }

   ctx->nir_ssa_values[instr->def.index] = reg;
}

static void
emit_undef(struct eir_context *ctx, nir_ssa_undef_instr *undef)
{
   compile_error(ctx, "Unhandled NIR instruction type: ssa_undef\n");
}

static void
emit_tex(struct eir_context *ctx, nir_tex_instr *tex)
{
   const nir_op_info *info = &nir_op_infos[tex->op];
   struct gc_instr *instr;
   struct gc_instr_src coordinate;

   struct gc_instr_dst dst;
   dst = get_nir_dest(ctx, &tex->dest);
   dst.comps = INST_COMPS_X | INST_COMPS_Y | INST_COMPS_Z | INST_COMPS_W;

   for (unsigned i = 0; i < tex->num_srcs; i++) {
      switch (tex->src[i].src_type) {
      case nir_tex_src_coord:
         coordinate = get_nir_src(ctx, &tex->src[i].src);
         coordinate.swiz = INST_SWIZ_IDENTITY;
         break;

      case nir_tex_src_projector:
         unreachable("Should be lowered by nir_lower_tex");
         break;

      default:
         compile_error(ctx, "Unhandled NIR tex src type: %d\n",
                       tex->src[i].src_type);
         return;
      }
   }

   switch (tex->op) {
   case nir_texop_tex:
      instr = eir_TEXLD(ctx->block, &dst, &coordinate);
      instr->sampler.id = tex->texture_index;
      instr->sampler.amode = GC_ADDRESSING_MODE_DIRECT;
      instr->sampler.swiz = INST_SWIZ_IDENTITY;
      break;

   case nir_texop_txb:
      instr = eir_TEXLDB(ctx->block, &dst, &coordinate);
      instr->sampler.id = tex->texture_index;
      instr->sampler.amode = GC_ADDRESSING_MODE_DIRECT;
      instr->sampler.swiz = INST_SWIZ_IDENTITY;
      break;

   case nir_texop_txl:
      instr = eir_TEXLDL(ctx->block, &dst, &coordinate);
      instr->sampler.id = tex->texture_index;
      instr->sampler.amode = GC_ADDRESSING_MODE_DIRECT;
      instr->sampler.swiz = INST_SWIZ_IDENTITY;
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
      emit_load_const(ctx, nir_instr_as_load_const(instr));
      break;
   case nir_instr_type_ssa_undef:
      emit_undef(ctx, nir_instr_as_ssa_undef(instr));
      break;
   case nir_instr_type_tex:
      emit_tex(ctx, nir_instr_as_tex(instr));
      break;
   case nir_instr_type_jump:
      emit_jump(ctx, nir_instr_as_jump(instr));
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

   struct gc_instr_src condition = get_nir_src(ctx, &nif->condition);
   struct gc_instr_src s0 = eir_uniform_u32(ctx->ir, 0);
   struct gc_instr *instr = eir_BRANCH(ctx->block, &condition, &s0);
   instr->condition = GC_COND_GT;

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
   compile_error(ctx, "Unhandled NIR loop\n");
}

static void
emit_cf_list(struct eir_context *ctx, struct exec_list *list)
{
   assert(!exec_list_is_empty(list));

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

   /* let's pretend things other than vec4 don't exist */
   compile_assert(ctx, ncomp == 4);
#if 0
   if (ctx->s->info.stage == MESA_SHADER_FRAGMENT) {
      compile_error(ctx, "unknown FS output name: %s\n",
            gl_frag_result_name(slot));
   } else if (ctx->s->info.stage == MESA_SHADER_VERTEX) {
      compile_error(ctx, "unknown VS output name: %s\n",
            gl_varying_slot_name(slot));
   } else {
      compile_error(ctx, "unknown shader type: %d\n", ctx->s->info.stage);
   }
#endif
}

static void
setup_output(struct eir_context *ctx, nir_variable *out)
{
   struct eir_shader_variant *v = ctx->variant;
   unsigned array_len = MAX2(glsl_get_length(out->type), 1);
   unsigned ncomp = glsl_get_components(out->type);
   unsigned n = out->data.driver_location;
   unsigned slot = out->data.location;

   DBG(ctx->compiler, "; out: slot=%u, len=%ux%u, drvloc=%u",
         slot, array_len, ncomp, n);

   /* let's pretend things other than vec4 don't exist */
   compile_assert(ctx, ncomp == 4);

   if (ctx->s->info.stage == MESA_SHADER_FRAGMENT) {
      switch (slot) {
      case FRAG_RESULT_DEPTH:
         v->fs_depth_out_reg = n;
         break;

      case FRAG_RESULT_COLOR:
         v->fs_color_out_reg = n;
         break;

      default:
            compile_error(ctx, "unknown FS output name: %s\n",
                  gl_frag_result_name(slot));
      }
   } else if (ctx->s->info.stage == MESA_SHADER_VERTEX) {
      switch (slot) {
      case VARYING_SLOT_POS:
         v->vs_pos_out_reg = n;
         break;

      case VARYING_SLOT_PSIZ:
         v->vs_pointsize_out_reg = n;
         break;

      default:
         compile_error(ctx, "unknown VS output name: %s\n",
               gl_varying_slot_name(slot));
      }
   } else {
      compile_error(ctx, "unknown shader type: %d\n", ctx->s->info.stage);
   }
}

static void
emit_instructions(struct eir_context *ctx)
{
   nir_function_impl *fxn = nir_shader_get_entrypoint(ctx->s);

   ctx->ir->num_temps = fxn->reg_alloc;

   if (ctx->s->num_uniforms > 0)
      ctx->uniforms = ctx->s->num_uniforms / 16;

   nir_foreach_variable(var, &ctx->s->inputs)
      setup_input(ctx, var);

   nir_foreach_variable(var, &ctx->s->outputs)
      setup_output(ctx, var);

   ctx->nir_ssa_values = ralloc_array(ctx, struct gc_instr_src, fxn->ssa_alloc);

   emit_function(ctx, fxn);
}

static void
copy_uniform_state_to_shader(struct eir_uniform_info *uniforms, struct eir_shader_variant *v)
{
   const unsigned count = uniforms->count;
   struct eir_uniform_info *uinfo = &v->uniforms;

   uinfo->count = count;
   uinfo->contents = mem_dup(uniforms->contents, count * sizeof(*uniforms->contents));
   uinfo->data = mem_dup(uniforms->data, count * sizeof(*uniforms->data));
}

int
eir_compile_shader_nir(struct eir_compiler *compiler,
                       struct eir_shader_variant *v)
{
   int ret = 0;
   struct eir_context *ctx;
   bool success;

   assert(!v->ir);

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

   if (compiler->debug & EIR_DBG_OPTMSGS) {
      printf("AFTER emitting:\n");
      eir_dump(ctx->ir);
   }

   eir_legalize(ctx->ir);
   if (compiler->debug & EIR_DBG_OPTMSGS) {
      printf("AFTER legalization:\n");
      eir_dump(ctx->ir);
   }

   eir_calculate_live_intervals(ctx->ir);
   if (compiler->debug & EIR_DBG_OPTMSGS) {
      printf("AFTER live-ranges:\n");
      eir_dump(ctx->ir);
   }

   success = eir_register_allocate(ctx->ir, ctx->compiler);
   if (!success) {
      DBG(compiler, "RA failed!");
      ret = -1;
      goto out;
   }

   if (compiler->debug & EIR_DBG_OPTMSGS) {
      printf("AFTER RA:\n");
      eir_dump(ctx->ir);
   }

   v->num_temps = ctx->ir->num_temps;
   v->const_size = ctx->ir->const_size;

   copy_uniform_state_to_shader(&ctx->ir->uniforms, v);

out:
   if (ret) {
      if (v->ir)
         eir_destroy(v->ir);
      v->ir = NULL;
   }

   compile_free(ctx);

   return ret;
}
