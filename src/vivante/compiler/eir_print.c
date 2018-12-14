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

#include <stdio.h>

#include "eir.h"
#include "vivante/gc/gc_disasm.h"

static void
tab(struct gc_string *string, const unsigned lvl)
{
	for (unsigned i = 0; i < lvl; i++)
      gc_string_append(string, "\t");
}

static void
print_a(struct gc_string *string, const struct eir *ir, unsigned ip, bool start)
{
   const int num = util_dynarray_num_elements(&ir->reg_alloc, unsigned);
   const int num_components = num * 4;
   bool first = true;

   for (int i = 0; i < num_components; i = i + 4) {
      unsigned comp = 0;

      for (int j = 0; j < 4; j++)
         if (ir->temp_start[i + j] == ip)
            comp |= 1 << j;

      if (comp == 0)
         continue;

      if (first) {
         first = false;
      } else {
         gc_string_append(string, ", ");
      }

      if (start)
         gc_string_append(string, "S%4d", i / 4);
      else
         gc_string_append(string, "E%4d", i / 4);

      gc_decode_components(string, comp);
   }

   if (first)
      gc_string_append(string, "      ");
   else
      gc_string_append(string, " ");
}

static void
print_live_ranges(struct gc_string *string, const struct eir *ir,
                  unsigned ip)
{
   print_a(string, ir, ip, true);
   print_a(string, ir, ip, false);
}

static void
print_register(struct gc_string *string, const struct eir_register *reg)
{
   switch (reg->type) {
   case EIR_REG_TEMP:
      gc_string_append(string, "t%d", reg->index);
      break;
   case EIR_REG_UNIFORM:
      gc_string_append(string, "u%d", reg->index);
     break;
   case EIR_REG_UNDEF:
      unreachable("undef??");
      break;
   default:
      unreachable("oops");
      break;
   }
}

static void
print_dst(struct gc_string *string, const struct eir_register *reg)
{
   print_register(string, reg);
   gc_decode_components(string, reg->writemask);
   gc_string_append(string, " ");
}

static void
print_src(struct gc_string *string, const struct eir_register *reg)
{
   if (reg->abs)
      gc_string_append(string, "|");

   print_register(string, reg);
   gc_decode_swiz(string, reg->swizzle);

   if (reg->abs)
      gc_string_append(string, "|");

   gc_string_append(string, " ");
}

static void
print_alu(struct gc_string *string, struct eir_instruction *inst)
{
   for (unsigned i = 0; i < gc_op_num_src(inst->gc.opcode); i++)
      print_src(string, &inst->src[i]);
}

static void
print_branch(struct gc_string *string, struct eir_instruction *inst)
{
   print_src(string, &inst->src[0]);
   print_src(string, &inst->src[1]);
   gc_string_append(string, "block %u", inst->block->successors[0]->num);
}

static void
print_sampler(struct gc_string *string, struct eir_instruction *inst)
{
   const struct eir_register *tex = &inst->src[0];

   gc_string_append(string, "tex%u", tex->index);
   /*gc_decode_address_mode(string, sampler->amode);*/
   gc_decode_swiz(string, tex->swizzle);
   gc_string_append(string, ", ");
   print_src(string, &inst->src[1]);
}

static void
print_block(struct gc_string *string, struct eir_block *block, unsigned ip, unsigned lvl)
{
   string->offset = 0;

   tab(string, lvl);
   gc_string_append(string, "block: %u\n", block->num);

   eir_for_each_inst(inst, block) {
      const size_t offset = string->offset;

      tab(string, lvl + 1);
      gc_string_append(string, "%s ", gc_op_name(inst->gc.opcode));
      gc_string_pad_to(string, offset, 11);

      if (gc_op_has_dst(inst->gc.opcode))
         print_dst(string, &inst->dst);
      else
         gc_string_append(string, "void ");

      const enum gc_op_type type = gc_op_type(inst->gc.opcode);
      switch (type) {
      case GC_OP_TYPE_ALU:
         print_alu(string, inst);
         break;
      case GC_OP_TYPE_BRANCH:
         print_branch(string, inst);
         break;
      case GC_OP_TYPE_SAMPLER:
         print_sampler(string, inst);
         break;
      }

      if (block->ir->temp_start) {
         gc_string_pad_to(string, offset, 64);
         print_live_ranges(string, block->ir, ip);
      }

      gc_string_append(string, "\n");
      ip++;
   }

   if (block->successors[1]) {
      tab(string, lvl);
      gc_string_append(string, "-> BLOCK %d, %d\n",
              block->successors[0]->num,
              block->successors[1]->num);
   } else if (block->successors[0]) {
      tab(string, lvl);
      gc_string_append(string, "-> BLOCK %d\n",
              block->successors[0]->num);
   }

   fprintf(stderr, "%s", string->string);
}

void
eir_print(struct eir *ir)
{
   assert(ir);
   unsigned ip = 0;

   struct gc_string string = {
      .string = (char *)rzalloc_size(NULL, 1),
      .offset = 0,
   };

   eir_for_each_block(block, ir)
      print_block(&string, block, ip, 0);

   ralloc_free(string.string);
}
