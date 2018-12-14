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
#include "eir_compiler.h"
#include "util/ralloc.h"
#include "util/u_debug.h"

static const struct debug_named_value shader_debug_options[] = {
   {"disasm",  EIR_DBG_DISASM, "Dump NIR and etnaviv shader disassembly"},
   {"optmsgs", EIR_DBG_OPTMSGS,"Enable optimizer debug messages"},
   DEBUG_NAMED_VALUE_END
};

DEBUG_GET_ONCE_FLAGS_OPTION(eir_compiler_debug, "EIR_COMPILER_DEBUG", shader_debug_options, 0)

enum eir_compiler_debug eir_compiler_debug = 0;

struct eir_compiler *
eir_compiler_create(void)
{
   struct eir_compiler *compiler = rzalloc(NULL, struct eir_compiler);

   if (!compiler)
      return NULL;

   eir_compiler_debug = debug_get_option_eir_compiler_debug();
   compiler->set = eir_ra_alloc_reg_set(compiler);

   return compiler;
}

void
eir_compiler_free(const struct eir_compiler *compiler)
{
   ralloc_free((void *)compiler);
}
