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
eir_uniform_register_vec4(struct eir *ir,
                          const unsigned ncomp,
                          const enum eir_uniform_contents *contents,
                          const uint32_t *values)
{
   const unsigned size = util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data);
   unsigned idx, i;

   /* is there already something useful to reuse? */
   for (idx = 0; idx < size; idx++) {
      const struct eir_uniform_data *data;

      for (i = 0; i < ncomp; i++) {
         data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, idx + i);

         if (data->content == EIR_UNIFORM_UNUSED)
            continue;

         if (data->content != contents[i] || data->data != values[i])
            break;
      }

		/* matched all components */
		if (i == ncomp)
			break;
   }

   /* do we need to update unused items? */
   if (idx != size) {
      struct eir_uniform_data *data;

      for (i = 0; i < ncomp; i++) {
         data = util_dynarray_element(&ir->uniform_alloc, struct eir_uniform_data, idx + i);

         data->content = contents[i];
         data->data = values[i];
      }
   }

	/* need to allocate new immediate */
	if (idx == size) {
      /* pad out current uniform vector - if needed */
      if (size % 4)
         for (i = size % 4; i < ncomp; i++)
            append_uniform(ir, EIR_UNIFORM_UNUSED, 0);

      for (i = 0; i < ncomp; i++)
         append_uniform(ir, contents[i], values[i]);

      idx = util_dynarray_num_elements(&ir->uniform_alloc, struct eir_uniform_data);
      idx -= ncomp;
   }

   idx += ir->uniform_offset;

   struct eir_register reg = {
      .type = EIR_REG_UNIFORM,
      .index = idx / 4,
   };

   if (ncomp == 4)
      reg.swizzle = INST_SWIZ_IDENTITY;
   else
      reg.swizzle = INST_SWIZ_BROADCAST(idx & 3);

   return reg;
}
