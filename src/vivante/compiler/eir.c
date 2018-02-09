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
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include "eir.h"

#include <stdlib.h>

#include "util/list.h"
#include "util/ralloc.h"

struct eir *
eir_create(void)
{
   struct eir *ir = rzalloc(NULL, struct eir);

   list_inithead(&ir->block_list);

   return ir;
}

void
eir_destroy(struct eir *ir)
{
   assert(ir);
   ralloc_free(ir);
}

struct eir_block *
eir_block_create(struct eir *ir)
{
   assert(ir);
   struct eir_block *block = rzalloc(ir, struct eir_block);

   block->ir = ir;
   block->num = ir->num_blocks++;
   list_inithead(&block->node);
   list_inithead(&block->instr_list);

   list_addtail(&block->node, &ir->block_list);

   return block;
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

struct gc_instr *
eir_instruction_create(struct eir_block *block, enum gc_op opc)
{
   assert(block);
   struct eir_instruction *instr = rzalloc(block, struct eir_instruction);

   instr->block = block;
   list_addtail(&instr->node, &block->instr_list);

   instr->gc.opcode = opc;

   return &instr->gc;
}

void
eir_update_ip_data(struct eir *ir)
{
   unsigned ip = 0;

   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      block->start_ip = ip;

      list_for_each_entry (struct eir_instruction, instr, &block->instr_list, node) {
         instr->ip = ip++;
      }

      block->end_ip = ip;
   }
}

uint32_t *
eir_assemble(struct eir *ir, struct eir_info *info)
{
   assert(ir);
   assert(info);
	uint32_t *ptr, *dwords;

   memset(info, 0, sizeof(*info));

   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      list_for_each_entry (struct eir_instruction, instr, &block->instr_list, node) {
         info->sizedwords += 4;
      }
   }

   ptr = dwords = calloc(4, info->sizedwords);

   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      list_for_each_entry (const struct eir_instruction, instr, &block->instr_list, node) {
         bool ok = gc_pack_instr(&instr->gc, dwords);

         if (!ok)
            goto fail;

         dwords += 4;
      }
   }

   return ptr;

fail:
   free(ptr);
   return NULL;
}

struct gc_instr_src
eir_uniform(struct eir *ir, enum eir_uniform_contents contents, uint32_t data)
{
   assert(ir);
   struct eir_uniform_info *uniforms = &ir->uniforms;
   unsigned idx;

   for (idx = 0; idx < uniforms->count; idx++)
      if (uniforms->contents[idx] == contents && uniforms->data[idx] == data)
            break;

   if (idx == uniforms->count) {
      /* see if there is enough space */
      if (idx == ir->uniforms_array_size) {
         int size = MAX2(16, idx * 2);

         uniforms->data = reralloc(ir, uniforms->data, uint32_t, size);
         uniforms->contents = reralloc(ir, uniforms->contents,
                                       enum eir_uniform_contents, size);

         ir->uniforms_array_size = size;
      }

      uniforms->contents[idx] = contents;
      uniforms->data[idx] = data;
      uniforms->count++;
   }

   idx += ir->const_size;

   struct gc_instr_src u = {
      .use = 1,
      .rgroup = GC_REGISTER_GROUP_UNIFORM,
      .reg = idx / 4,
      .swiz = INST_SWIZ_BROADCAST(idx & 3)
   };

   return u;
};

struct gc_instr_src
eir_uniform_vec4(struct eir *ir, enum eir_uniform_contents contents,
                 const uint32_t *values)
{
   assert(ir);
   struct eir_uniform_info *uniforms = &ir->uniforms;
   unsigned idx, i;

   for (idx = 0; idx + 3 < uniforms->count; idx += 4) {
      for (i = 0; i < 4; i++)
         if (uniforms->contents[idx + i] != contents || uniforms->data[idx + i] != values[i])
            break;
      if (i == 4)
         break;
   }

   if (idx >= uniforms->count) {
      /* see if there is enough space */
      if (idx >= ir->uniforms_array_size) {
         int size = MAX2(16, idx * 2);

         uniforms->data = reralloc(ir, uniforms->data, uint32_t, size);
         uniforms->contents = reralloc(ir, uniforms->contents,
                                       enum eir_uniform_contents, size);

         ir->uniforms_array_size = size;
      }

      for (i = 0; i < 4; i++) {
         uniforms->contents[idx + i] = contents;
         uniforms->data[idx + i] = values[i];
         uniforms->count++;
      }
   }

   idx += ir->const_size;

   struct gc_instr_src u = {
      .use = 1,
      .rgroup = GC_REGISTER_GROUP_UNIFORM,
      .reg = idx / 4,
      .swiz = INST_SWIZ_BROADCAST(idx & 3)
   };

   return u;
}

struct eir_uniform_info *
eir_uniform_info(struct eir *ir)
{
   assert(ir);
   return &ir->uniforms;
}
