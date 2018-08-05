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
   block->num = ir->blocks++;
   list_inithead(&block->node);
   list_inithead(&block->instr_list);

   list_addtail(&block->node, &ir->block_list);

   return block;
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
   if (!ptr)
      return NULL;

   list_for_each_entry (struct eir_block, block, &ir->block_list, node) {
      list_for_each_entry (const struct eir_instruction, instr, &block->instr_list, node) {
         gc_pack_instr(&instr->gc, dwords);
         dwords += 4;
      }
   }

   return ptr;
}
