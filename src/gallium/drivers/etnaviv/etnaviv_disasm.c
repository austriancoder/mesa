/*
 * Copyright (c) 2016-2018 Etnaviv Project
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

#include "etnaviv_disasm.h"
#include "util/ralloc.h"
#include "vivante/gc/gc_disasm.h"

#include <stdio.h>

void
etna_disasm(uint32_t *dwords, int sizedwords, enum debug_t debug)
{
   assert((sizedwords % 2) == 0);

   struct gc_string decoded = {
      .string = (char *)rzalloc_size(NULL, 1),
      .offset = 0,
   };

   for (unsigned i = 0; i < sizedwords; i += 4)
   {
      uint32_t *raw = &dwords[i];

      printf("%04d: ", i / 4);
      if (debug & PRINT_RAW)
         printf("%08x %08x %08x %08x  ", raw[0], raw[1], raw[2],
             raw[3]);

      gc_disasm(&decoded, raw);
      printf("%s\n", decoded.string);
   }

   ralloc_free(decoded.string);
}
