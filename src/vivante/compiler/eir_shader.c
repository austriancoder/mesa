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
#include "eir_compiler.h"
#include "eir_nir.h"
#include "eir_shader.h"
#include "gc/gc_disasm.h"

#include "util/u_memory.h"
#include "util/ralloc.h"

static inline bool
eir_shader_key_equal(struct eir_shader_key *a, struct eir_shader_key *b)
{
   STATIC_ASSERT(sizeof(struct eir_shader_key) <= sizeof(a->global));

   return a->global == b->global;
}

static void
delete_variant(struct eir_shader_variant *v)
{
   if (v->ir)
      eir_destroy(v->ir);

   if (v->code)
      free(v->code);

   FREE(v);
}

static inline void
assemble_variant(struct eir_shader_variant *v)
{
   v->code = eir_assemble(v->ir, &v->info);

	/* no need to keep the ir around beyond this point */
	eir_destroy(v->ir);
	v->ir = NULL;
}

static struct eir_shader_variant *
create_variant(struct eir_shader *shader, struct eir_shader_key key)
{
   struct eir_shader_variant *v = CALLOC_STRUCT(eir_shader_variant);
   int ret;

   if (!v)
      return NULL;

   v->id = shader->variant_count++;
   v->shader = shader;
   v->key = key;

   ret = eir_compile_shader_nir(shader->compiler, v);
   if (ret) {
      debug_error("compile failed!");
      goto fail;
   }

   assemble_variant(v);
   if (!v->code) {
      debug_error("assemble failed!");
      goto fail;
   }

   return v;

fail:
   delete_variant(v);
   return NULL;
}

/**
 * Returns the minimum number of vec4 elements needed to pack a type.
 *
 * For simple types, it will return 1 (a single vec4); for matrices, the
 * number of columns; for array and struct, the sum of the vec4_size of
 * each of its elements; and for sampler and atomic, zero.
 *
 * This method is useful to calculate how much register space is needed to
 * store a particular type.
 */
static int
eir_type_size_vec4(const struct glsl_type *type)
{
   switch (glsl_get_base_type(type)) {
   case GLSL_TYPE_UINT:
   case GLSL_TYPE_INT:
   case GLSL_TYPE_FLOAT:
   case GLSL_TYPE_FLOAT16:
   case GLSL_TYPE_BOOL:
   case GLSL_TYPE_DOUBLE:
   case GLSL_TYPE_UINT16:
   case GLSL_TYPE_INT16:
   case GLSL_TYPE_UINT8:
   case GLSL_TYPE_INT8:
   case GLSL_TYPE_UINT64:
   case GLSL_TYPE_INT64:
      if (glsl_type_is_matrix(type)) {
         const struct glsl_type *col_type = glsl_get_column_type(type);
         unsigned col_slots = glsl_type_is_dual_slot(col_type) ? 2 : 1;
         return glsl_get_matrix_columns(type) * col_slots;
      } else {
         /* Regardless of size of vector, it gets a vec4. This is bad
          * packing for things like floats, but otherwise arrays become a
          * mess. Hopefully a later pass over the code can pack scalars
          * down if appropriate.
          */
         return glsl_type_is_dual_slot(type) ? 2 : 1;
      }
   case GLSL_TYPE_ARRAY:
   case GLSL_TYPE_STRUCT:
   case GLSL_TYPE_SUBROUTINE:
   case GLSL_TYPE_SAMPLER:
   case GLSL_TYPE_ATOMIC_UINT:
   case GLSL_TYPE_IMAGE:
   case GLSL_TYPE_VOID:
   case GLSL_TYPE_ERROR:
   case GLSL_TYPE_INTERFACE:
   case GLSL_TYPE_FUNCTION:
   default:
      unreachable("todo");
   }

   return 0;
}

struct eir_shader *
eir_shader_from_nir(struct eir_compiler *compiler, struct nir_shader *nir)
{
   struct eir_shader *shader = CALLOC_STRUCT(eir_shader);

   assert(compiler);

   shader->mem_ctx = ralloc_context(NULL);
   shader->compiler = compiler;
   shader->id = shader->compiler->shader_count++;
   shader->type = nir->info.stage;

   NIR_PASS_V(nir, nir_lower_io, nir_var_all, eir_type_size_vec4,
         (nir_lower_io_options)0);

   shader->nir = eir_optimize_nir(nir);

   return shader;
}

void
eir_shader_destroy(struct eir_shader *shader)
{
   struct eir_shader_variant *v, *t;

   assert(shader);

   v = shader->variants;
   while (v) {
      t = v;
      v = v->next;
      delete_variant(t);
   }

   ralloc_free(shader->nir);
   ralloc_free(shader->mem_ctx);
   FREE(shader);
}

struct eir_shader_variant *
eir_shader_get_variant(struct eir_shader *shader, struct eir_shader_key key, bool *created)
{
   struct eir_shader_variant *v;

   *created = false;

   for (v = shader->variants; v; v = v->next)
      if (eir_shader_key_equal(&key, &v->key))
         return v;

   /* compile new variant if it doesn't exist already */
   v = create_variant(shader, key);
   if (v) {
      v->next = shader->variants;
      shader->variants = v;
      *created = true;
   }

   return v;
}

static const char *
swizzle_names[4] =
{
   "x",
   "y",
   "z",
   "w"
};

void
eir_dump_shader(struct eir_shader_variant *v)
{
   assert(v);
   assert(v->shader);
   assert((v->info.sizedwords % 2) == 0);
   unsigned i;

   printf("%s\n", _mesa_shader_stage_to_string(v->shader->type));

   struct gc_string decoded = {
      .string = (char *)rzalloc_size(NULL, 1),
      .offset = 0,
   };

   for (i = 0; i < v->info.sizedwords; i += 4) {
      const uint32_t *raw = &v->code[i];
      gc_disasm(&decoded, raw);

      printf("%04d: %08x %08x %08x %08x  %s\n",
            i / 4,
            raw[0], raw[1], raw[2], raw[3],
            decoded.string);
   }

   ralloc_free(decoded.string);

   printf("num loops: 0\n");
   printf("num temps: %i\n", v->num_temps);
   printf("num const: %i\n", v->const_size);

   printf("immediates:\n");
   i = 0;
   util_dynarray_foreach(&v->uniforms, struct eir_uniform_data, uniform) {
      printf(" u%i.%s = 0x%08x\n",
             (v->const_size + i) / 4,
             swizzle_names[i % 4],
             uniform->data);

      i++;
   }

   printf("inputs:\n");
   printf("outputs:\n");

   printf("special:\n");
   if (v->shader->type == MESA_SHADER_VERTEX) {
      printf("  vs_pos_out_reg=%i\n", v->vs_pos_out_reg);
      printf("  vs_pointsize_out_reg=%i\n", v->vs_pointsize_out_reg);
      //printf("  vs_load_balancing=0x%08x\n", shader->vs_load_balancing);
   } else {
      printf("  ps_color_out_reg=%i\n", v->fs_color_out_reg);
      printf("  ps_depth_out_reg=%i\n", v->fs_depth_out_reg);
   }
   //printf("  input_count_unk8=0x%08x\n", shader->input_count_unk8);
}
