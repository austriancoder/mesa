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
#include "gc/gc_disasm.h"
#include <stdio.h>
#include "util/list.h"

static void
tab(unsigned lvl)
{
   for (unsigned i = 0; i < lvl; i++)
      fprintf(stderr, "\t");
}

static void
print_live_ranges(const struct eir *ir, unsigned ip, unsigned len)
{
   bool first;

   /* make some space for nice readable output */
   while (len < 64) {
      fprintf(stderr, " ");
      len++;
   }

   /* start */
   first = true;

   for (int i = 0; i < ir->num_temps; i++) {
      if (ir->temp_start[i] != ip)
         continue;

      if (first) {
         first = false;
      } else {
         fprintf(stderr, ", ");
      }
      fprintf(stderr, "S%4d", i);
   }

   if (first)
      fprintf(stderr, "      ");
   else
      fprintf(stderr, " ");


   /* end */
   first = true;

   for (int i = 0; i < ir->num_temps; i++) {
      if (ir->temp_end[i] != ip)
         continue;

      if (first) {
         first = false;
      } else {
         fprintf(stderr, ", ");
      }
      fprintf(stderr, "E%4d", i);
   }

   if (first)
         fprintf(stderr, "      ");
   else
         fprintf(stderr, " ");
}

static unsigned
print_block(const struct eir_block *block, unsigned ip, unsigned lvl)
{
   const struct eir *ir = block->ir;

   tab(lvl);
   printf("block: %u\n", block->num);

   list_for_each_entry(struct eir_instruction, instr, &block->instr_list, node) {
      tab(lvl + 1);
      int len = gc_dump(&instr->gc);

      if (ir->live_intervals_valid)
         print_live_ranges(ir, ip, len);

      fprintf(stderr, "\n");
      ip++;
   }

   if (block->successors[1]) {
      tab(lvl);
      fprintf(stderr, "-> BLOCK %d, %d\n",
              block->successors[0]->num,
              block->successors[1]->num);
   } else if (block->successors[0]) {
      tab(lvl);
      fprintf(stderr, "-> BLOCK %d\n",
              block->successors[0]->num);
   }

   return ip;
}

void
eir_dump(struct eir *ir)
{
   assert(ir);
   unsigned ip = 0;

   list_for_each_entry(struct eir_block, block, &ir->block_list, node)
      ip = print_block(block, ip, 1);
}
