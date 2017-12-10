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
#include "util/ralloc.h"

struct eir *
eir_create(void)
{
   struct eir *ir = rzalloc(NULL, struct eir);

   return ir;
}

void
eir_destroy(struct eir *ir)
{
   assert(ir);
   ralloc_free(ir);
}

struct etna_inst_src
eir_immediate(struct eir *ir, enum etna_immediate_contents contents,
              uint32_t data)
{
   assert(ir);
   int idx;

   /* Could use a hash table to speed this up */
   for (idx = 0; idx < ir->imm_size; ++idx) {
      if (ir->imm_contents[idx] == contents && ir->imm_data[idx] == data)
         break;
   }

   /* look if there is an unused slot */
   if (idx == ir->imm_size) {
      for (idx = 0; idx < ir->imm_size; ++idx) {
         if (ir->imm_contents[idx] == ETNA_IMMEDIATE_UNUSED)
            break;
      }
   }

   /* allocate new immediate */
   if (idx == ir->imm_size) {
      assert(ir->imm_size < ETNA_MAX_IMM);
      idx = ir->imm_size++;
      ir->imm_data[idx] = data;
      ir->imm_contents[idx] = contents;
   }

   /* swizzle so that component with value is returned in all components */
   idx += ir->imm_base;
   struct etna_inst_src imm_src = {
      .use = 1,
      .rgroup = INST_RGROUP_UNIFORM_0,
      .reg = idx / 4,
      .swiz = INST_SWIZ_BROADCAST(idx & 3)
   };

   return imm_src;
}

struct etna_inst_src
eir_immediate_vec4(struct eir *ir, enum etna_immediate_contents contents,
                   const uint32_t *values)
{
   assert(ir);
   assert(values);
   struct etna_inst_src imm_src = { };
   int idx, i;

   for (idx = 0; idx + 3 < ir->imm_size; idx += 4) {
      /* What if we can use a uniform with a different swizzle? */
      for (i = 0; i < 4; i++)
         if (ir->imm_contents[idx + i] != contents || ir->imm_data[idx + i] != values[i])
            break;
      if (i == 4)
         break;
   }

   if (idx + 3 >= ir->imm_size) {
      idx = align(ir->imm_size, 4);
      assert(idx + 4 <= ETNA_MAX_IMM);

      for (i = 0; i < 4; i++) {
         ir->imm_data[idx + i] = values[i];
         ir->imm_contents[idx + i] = contents;
      }

      ir->imm_size = idx + 4;
   }

   assert((ir->imm_base & 3) == 0);
   idx += ir->imm_base;
   imm_src.use = 1;
   imm_src.rgroup = INST_RGROUP_UNIFORM_0;
   imm_src.reg = idx / 4;
   imm_src.swiz = INST_SWIZ_IDENTITY;

   return imm_src;
}
