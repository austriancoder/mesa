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

#include "eir.h"
#include <stdlib.h>
#include <string.h>
#include "util/ralloc.h"

struct eir *
eir_create(void)
{
   struct eir *ir = rzalloc(NULL, struct eir);

   list_inithead(&ir->block_list);
   util_dynarray_init(&ir->reg_alloc, ir);
   util_dynarray_init(&ir->uniform_alloc, ir);

   return ir;
}

void
eir_destroy(struct eir *ir)
{
   assert(ir);
   util_dynarray_fini(&ir->reg_alloc);
   util_dynarray_fini(&ir->uniform_alloc);
   ralloc_free(ir);
}

struct eir_block *
eir_block_create(struct eir *ir)
{
   assert(ir);
   struct eir_block *block = rzalloc(ir, struct eir_block);

   block->ir = ir;
   block->num = ir->blocks++;
   list_inithead(&block->node);
   list_inithead(&block->instr_list);

   list_addtail(&block->node, &ir->block_list);

   return block;
}

struct eir_instruction *
eir_instruction_create(struct eir_block *block, enum gc_op opc)
{
   assert(block);
   struct eir_instruction *instr = rzalloc(block, struct eir_instruction);

   instr->block = block;
   list_addtail(&instr->node, &block->instr_list);

   instr->gc.opcode = opc;

   return instr;
}

struct eir_register
eir_temp_register(struct eir *ir, unsigned num_components)
{
   assert(num_components >= 1 && num_components <= 4);

   struct eir_register reg = {
      .type = EIR_REG_TEMP,
      .index = util_dynarray_num_elements(&ir->reg_alloc, unsigned),
   };

   util_dynarray_append(&ir->reg_alloc, unsigned, num_components);

   return reg;
}

static inline void
append_uniform(struct eir *ir, enum eir_uniform_contents content, uint32_t value)
{
   struct eir_uniform_data d = {
      .content = content,
      .data = value
   };

   util_dynarray_append(&ir->uniform_alloc, struct eir_uniform_data, d);
}

struct eir_register
eir_uniform_register(struct eir *ir, enum eir_uniform_contents content, uint32_t value)
{
   const unsigned size = util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data);
   unsigned i = 0;

   /* is there already something useful in the array? */
   util_dynarray_foreach(&ir->uniform_alloc, struct eir_uniform_data, uniform) {
      if (uniform->content == content && uniform->data == value)
         break;

      i++;
   }

   if (i == size)
      append_uniform(ir, content, value);

   i += ir->uniform_offset;

   struct eir_register reg = {
      .type = EIR_REG_UNIFORM,
      .index = i / 4,
      .swizzle = INST_SWIZ_BROADCAST(i & 3)
   };

   return reg;
}

struct eir_register
eir_uniform_register_vec4(struct eir *ir, enum eir_uniform_contents content, const uint32_t *values)
{
   const unsigned size = util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data);
   const struct eir_uniform_data *data;
   unsigned i, component = 0;

   /* is there already something useful in the array? */
   for (i = 0; i < size; i += 4) {
      for (component = 0; component < 4; component++) {
         data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, i + component);

         if (data->content != content || data->data != values[component])
            break;
      }

      if (component == 4)
         break;
   }

   if (component != 4)
      for (component = 0; component < 4; component++)
         append_uniform(ir, content, values[component]);

   i = util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data) - 4;
   i += ir->uniform_offset;

   struct eir_register reg = {
      .type = EIR_REG_UNIFORM,
      .index = i / 4,
      .swizzle = INST_SWIZ_IDENTITY
   };

   return reg;
}

void
eir_link_blocks(struct eir_block *predecessor, struct eir_block *successor)
{
   assert(predecessor);
   assert(successor);

   if (predecessor->successors[0]) {
      assert(!predecessor->successors[1]);
      predecessor->successors[1] = successor;
   } else {
      predecessor->successors[0] = successor;
   }
}

static void
convert_sampler(const struct eir_register *eir, struct gc_instr_sampler *sampler)
{
   assert(eir->type == EIR_REG_SAMPLER);

   sampler->id = eir->index;
   sampler->swiz = eir->swizzle;
   sampler->amode = GC_ADDRESSING_MODE_DIRECT;
}

static void
convert_src(const struct eir_register *eir, struct gc_instr_src *gc)
{
   assert(!gc->use);
   assert(eir->type != EIR_REG_UNDEF);
   assert(eir->type != EIR_REG_SAMPLER);

   gc->use = true;
   gc->reg = eir->index;
   gc->abs = eir->abs;
   gc->neg = eir->neg;
   gc->swiz = eir->swizzle;

   switch (eir->type) {
   case EIR_REG_TEMP:
      gc->rgroup = GC_REGISTER_GROUP_TEMP;
      break;

   case EIR_REG_UNIFORM:
      gc->rgroup = GC_REGISTER_GROUP_UNIFORM;
      break;

   default:
      unreachable("unhandled type");
   }
}

static void
convert_dst(const struct eir_register *eir, struct gc_instr_dst *gc)
{
   assert(!gc->comps);

   gc->reg = eir->index;
   gc->comps = eir->writemask;
}

static void
eir_to_gc(struct eir_instruction *instr)
{
   struct gc_instr *gc = &instr->gc;

   if (gc_op_num_src(gc->opcode)) {
      const unsigned swizzle = gc_op_src_swizzle(gc->opcode);

      /* src swizzle */
      for (int i = 0; i < gc_op_num_src(gc->opcode); i++) {
         const unsigned shift = i * 2;
         const unsigned mask = 0x3 << shift;
         const unsigned index = (swizzle & mask) >> shift;

         if (index == 0x3)
            continue;

         if (instr->src[i].type == EIR_REG_UNDEF)
            continue;

         switch (gc->type) {
         case GC_OP_TYPE_ALU:
            convert_src(&instr->src[i], &gc->alu.src[index]);
            break;

         case GC_OP_TYPE_BRANCH:
            convert_src(&instr->src[i], &gc->branch.src[index]);
            break;

         case GC_OP_TYPE_SAMPLER:
            if (i == 0)
               convert_sampler(&instr->src[0], &gc->sampler);
            else if (i == 1)
               convert_src(&instr->src[1], &gc->sampler.src);
            else
               unreachable("unkown sampler index");

            break;
         }
      }

      /* special handling for src replication */
      if (gc->type == GC_OP_TYPE_ALU && swizzle & SWIZ_REP_SRC0_TO_SRC2) {
         assert(gc->type == GC_OP_TYPE_ALU);
         convert_src(&instr->src[0], &gc->alu.src[2]);
      }
   }

   if (gc_op_has_dst(gc->opcode))
      convert_dst(&instr->dst, &gc->dst);
}

uint32_t *
eir_assemble(struct eir *ir, struct eir_info *info)
{
   assert(ir);
   assert(info);
   uint32_t *ptr, *dwords;

   memset(info, 0, sizeof(*info));

   eir_for_each_block(block, ir) {
      eir_for_each_inst(instr, block) {
         info->sizedwords += 4;
      }
   }

   ptr = dwords = calloc(4, info->sizedwords);
   if (!ptr)
      return NULL;

   eir_for_each_block(block, ir) {
      eir_for_each_inst(instr, block) {
         eir_to_gc(instr);
         gc_pack_instr(&instr->gc, dwords);
         dwords += 4;
      }
   }

   return ptr;
}
