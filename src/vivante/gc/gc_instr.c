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

#include "gc_instr.h"
#include <stddef.h>
#include "util/macros.h"

const char *
gc_op_name(enum gc_op op)
{
   static const char *op_names[] = {
      [GC_NOP] = "nop",
      [GC_ADD] = "add",
      [GC_MAD] = "mad",
      [GC_MUL] = "mul",
      [GC_DST] = "dst",
      [GC_DP3] = "dp3",
      [GC_DP4] = "dp4",
      [GC_DSX] = "dsx",
      [GC_DSY] = "dsy",
      [GC_MOV] = "mov",
      [GC_MOVAR] = "movar",
      [GC_MOVAF] = "movaf",
      [GC_RCP] = "rcp",
      [GC_RSQ] = "rsq",
      [GC_LITP] = "litp",
      [GC_SELECT] = "select",
      [GC_SET] = "set",
      [GC_EXP] = "exp",
      [GC_LOG] = "log",
      [GC_FRC] = "frc",
      [GC_CALL] = "call",
      [GC_RET] = "ret",
      [GC_BRANCH] = "branch",
      [GC_TEXKILL] = "texkill",
      [GC_TEXKILL_IF] = "texkill if",
      [GC_TEXLD] = "texld",
      [GC_TEXLDB] = "texldb",
      [GC_TEXLDD] = "texldd",
      [GC_TEXLDL] = "texldl",
      [GC_TEXLDPCF] = "texldpcf",
      [GC_REP] = "rep",
      [GC_ENDREP] = "endrep",
      [GC_LOOP] = "loop",
      [GC_ENDLOOP] = "endloop",
      [GC_SQRT] = "sqrt",
      [GC_SIN] = "sin",
      [GC_COS] = "cos",
      [GC_FLOOR] = "floor",
      [GC_CEIL] = "ceil",
      [GC_SIGN] = "sign",
      [GC_ADDLO] = "addlo",
      [GC_MULLO] = "addhi",
      [GC_BARRIER] = "barrier",
      [GC_SWIZZLE] = "swizzle",
      [GC_I2I] = "i2i",
      [GC_I2F] = "i2f",
      [GC_F2I] = "f2i",
      [GC_F2IRND] = "f2irnd",
      [GC_F2I7] = "f2i7",
      [GC_CMP] = "cmp",
      [GC_LOAD] = "load",
      [GC_STORE] = "store",
      [GC_COPYSIGN] = "copysign",
      [GC_GETEXP] = "getexp",
      [GC_GETMANT] = "getmant",
      [GC_NAN] = "nan",
      [GC_NEXTAFTER] = "nextafter",
      [GC_ROUNDEVEN] = "roundeven",
      [GC_ROUNDAWAY] = "roundaway",
      [GC_IADDSAT] = "iaddsat",
      [GC_IMULLO0] = "imull00",
      [GC_IMULLO1] = "imull01",
      [GC_IMULLOSAT0] = "imullosat0",
      [GC_IMULLOSAT1] = "imullosat1",
      [GC_IMULHI0] = "imulhi0",
      [GC_IMULHI1] = "imulhi1",
      [GC_IMUL0] = "imul0",
      [GC_IMUL1] = "imul1",
      [GC_IDIV0] = "idiv0",
      [GC_IDIV1] = "idiv1",
      [GC_IDIV2] = "idiv2",
      [GC_IDIV3] = "idiv3",
      [GC_IMOD0] = "imod0",
      [GC_IMOD1] = "imod1",
      [GC_IMOD2] = "imod2",
      [GC_IMOD3] = "imod3",
      [GC_IMADLO0] = "imadlo0",
      [GC_IMADLO1] = "imadlo1",
      [GC_IMADLOSAT0] = "imadlosat0",
      [GC_IMADLOSAT1] = "imadlosat1",
      [GC_IMADHI0] = "imadhi0",
      [GC_IMADHI1] = "imadhi1",
      [GC_IMADHISAT0] = "imadhisat0",
      [GC_IMADHISAT1] = "imadhisat1",
      [GC_HALFADD] = "halfadd",
      [GC_HALFADDINC] = "halfaddinc",
      [GC_MOVAI] = "movai",
      [GC_IABS] = "iabs",
      [GC_LEADZERO] = "leadzero",
      [GC_LSHIFT] = "lshift",
      [GC_RSHIFT] = "rshift",
      [GC_ROTATE] = "rotate",
      [GC_OR] = "or",
      [GC_AND] = "and",
      [GC_XOR] = "xor",
      [GC_NOT] = "not",
      [GC_BITSELECT] = "bitselect",
      [GC_POPCOUNT] = "popcount",
      [GC_STOREB] = "storeb",
      [GC_RGB2YUV] = "rgb2yuv",
      [GC_DIV] = "div",
      [GC_ATOM_ADD] = "atom_add",
      [GC_ATOM_XCHG] = "atom_xchg",
      [GC_ATOM_CMP_XCHG] = "atom_cmp_xchg",
      [GC_ATOM_MIN] = "atom_min",
      [GC_ATOM_MAX] = "atom_max",
      [GC_ATOM_OR] = "atom_or",
      [GC_ATOM_AND] = "atom_and",
      [GC_ATOM_XOR] = "atom_xor",
      [GC_BIT_REV] = "bit_rev",
      [GC_BYTE_REV] = "byte_rev",
      [GC_TEXLDLPCF] = "texldlpcf",
      [GC_TEXLDGPCF] = "texldgpcf",
      [GC_PACK] = "pack",
      [GC_CONV] = "conv",
      [GC_DP2] = "dp2",
      [GC_NORM_DP2] = "norm_dp2",
      [GC_NORM_DP3] = "norm_dp3",
      [GC_NORM_DP4] = "norm_dp4",
      [GC_NORM_MUL] = "norm_mul",
      [GC_STORE_ATTR] = "store_attr",
      [GC_LOAD_ATTR] = "load_attr",
      [GC_EMIT] = "emit",
      [GC_RESTART] = "restart",
      [GC_NOP7C] = "nop7c",
      [GC_NOP7D] = "nop7d",
      [GC_NOP7E] = "nop7e",
      [GC_NOP7F] = "nop7f",
   };

   if (op >= ARRAY_SIZE(op_names))
      return NULL;

   return op_names[op];
}

#define D	(1 << 0)
#define A	(1 << 1)
#define B	(1 << 2)
#define C	(1 << 3)
static const uint8_t op_args[] = {
   [GC_NOP] = 0,
   [GC_ADD] = D | B,
   [GC_MAD] = D | C,
   [GC_MUL] = D | B,
   [GC_DST] = D | B,
   [GC_DP3] = D | B,
   [GC_DP4] = D | B,
   [GC_DSX] = D | B,
   [GC_DSY] = D | B,
   [GC_MOV] = D | A,
   [GC_MOVAR] = D | A,
   [GC_MOVAF] = D | A,
   [GC_RCP] = D | A,
   [GC_RSQ] = D | A,
   [GC_LITP] = D | B,
   [GC_SELECT] = D | C,
   [GC_SET] = D | B,
   [GC_EXP] = D | A,
   [GC_LOG] = D | A,
   [GC_FRC] = D | A,
   [GC_CALL] = 0, /* TODO */
   [GC_RET] = 0, /* TODO */
   [GC_BRANCH] = B,
   [GC_TEXKILL] = 0,
   [GC_TEXKILL_IF] = B,
   [GC_TEXLD] = D | B,
   [GC_TEXLDB] = D | B,
   [GC_TEXLDD] = 0, /* TODO */
   [GC_TEXLDL] = D | B,
   [GC_TEXLDPCF] = 0, /* TODO */
   [GC_REP] = 0, /* TODO */
   [GC_ENDREP] = 0, /* TODO */
   [GC_LOOP] = 0, /* TODO */
   [GC_ENDLOOP] = 0, /* TODO */
   [GC_SQRT] = D | A,
   [GC_SIN] = D | A,
   [GC_COS] = D | A,
   [GC_FLOOR] = D | A,
   [GC_CEIL] = D | A,
   [GC_SIGN] = D | A,
   [GC_ADDLO] = 0, /* TODO */
   [GC_MULLO] = 0, /* TODO */
   [GC_BARRIER] = 0, /* TODO */
   [GC_SWIZZLE] = 0, /* TODO */
   [GC_I2I] = 0, /* TODO */
   [GC_I2F] = D | A,
   [GC_F2I] = D | A,
   [GC_F2IRND] = 0, /* TODO */
   [GC_F2I7] = 0, /* TODO */
   [GC_CMP] = 0, /* TODO */
   [GC_LOAD] = D | B,
   [GC_STORE] = D | C,
   [GC_COPYSIGN] = 0, /* TODO */
   [GC_GETEXP] = 0, /* TODO */
   [GC_GETMANT] = 0, /* TODO */
   [GC_NAN] = 0, /* TODO */
   [GC_NEXTAFTER] = 0, /* TODO */
   [GC_ROUNDEVEN] = 0, /* TODO */
   [GC_ROUNDAWAY] = 0, /* TODO */
   [GC_IADDSAT] = 0, /* TODO */
   [GC_IMULLO0] = D | B,
   [GC_IMULLO1] = 0, /* TODO */
   [GC_IMULLOSAT0] = 0, /* TODO */
   [GC_IMULLOSAT1] = 0, /* TODO */
   [GC_IMULHI0] = D | B,
   [GC_IMULHI1] = D, /* TODO */
   [GC_IMUL0] = 0, /* TODO */
   [GC_IMUL1] = 0, /* TODO */
   [GC_IDIV0] = 0, /* TODO */
   [GC_IDIV1] = 0, /* TODO */
   [GC_IDIV2] = 0, /* TODO */
   [GC_IDIV3] = 0, /* TODO */
   [GC_IMOD0] = 0, /* TODO */
   [GC_IMOD1] = 0, /* TODO */
   [GC_IMOD2] = 0, /* TODO */
   [GC_IMOD3] = 0, /* TODO */
   [GC_IMADLO0] = D | C,
   [GC_IMADLO1] =  0, /* TODO */
   [GC_IMADLOSAT0] = 0, /* TODO */
   [GC_IMADLOSAT1] = 0, /* TODO */
   [GC_IMADHI0] = 0, /* TODO */
   [GC_IMADHI1] = 0, /* TODO */
   [GC_IMADHISAT0] = 0, /* TODO */
   [GC_IMADHISAT1] = 0, /* TODO */
   [GC_HALFADD] = 0, /* TODO */
   [GC_HALFADDINC] = 0, /* TODO */
   [GC_MOVAI] = 0, /* TODO */
   [GC_IABS] = 0, /* TODO */
   [GC_LEADZERO] = D | A,
   [GC_LSHIFT] = D | B,
   [GC_RSHIFT] = D | B,
   [GC_ROTATE] = D | B,
   [GC_OR] = D | B,
   [GC_AND] = D | B,
   [GC_XOR] = D | B,
   [GC_NOT] = D | B,
   [GC_BITSELECT] = 0, /* TODO */
   [GC_POPCOUNT] = D | A,
   [GC_STOREB] = 0, /* TODO */
   [GC_RGB2YUV] = 0, /* TODO */
   [GC_DIV] = D | B,
   [GC_ATOM_ADD] = 0, /* TODO */
   [GC_ATOM_XCHG] = 0, /* TODO */
   [GC_ATOM_CMP_XCHG] = 0, /* TODO */
   [GC_ATOM_MIN] = 0, /* TODO */
   [GC_ATOM_MAX] = 0, /* TODO */
   [GC_ATOM_OR] = 0, /* TODO */
   [GC_ATOM_AND] = 0, /* TODO */
   [GC_ATOM_XOR] = 0, /* TODO */
   [GC_BIT_REV] = 0, /* TODO */
   [GC_BYTE_REV] = 0, /* TODO */
   [GC_TEXLDLPCF] = 0, /* TODO */
   [GC_TEXLDGPCF] = 0, /* TODO */
   [GC_PACK] = 0, /* TODO */
   [GC_CONV] = 0, /* TODO */
   [GC_DP2] = D | B,
   [GC_NORM_DP2] = 0, /* TODO */
   [GC_NORM_DP3] = 0, /* TODO */
   [GC_NORM_DP4] = 0, /* TODO */
   [GC_NORM_MUL] = 0, /* TODO */
   [GC_STORE_ATTR] = 0, /* TODO */
   [GC_LOAD_ATTR] = 0, /* TODO */
   [GC_EMIT] = 0, /* TODO */
   [GC_RESTART] = 0, /* TODO */
   [GC_NOP7C] = 0,
   [GC_NOP7D] = 0,
   [GC_NOP7E] = 0,
   [GC_NOP7F] = 0,
};

bool
gc_op_has_dst(enum gc_op op)
{
   assert(op < ARRAY_SIZE(op_args));

   return op_args[op] & D;
}

int
gc_op_num_src(enum gc_op op)
{
   assert(op < ARRAY_SIZE(op_args));
   const uint8_t args = op_args[op];

   if (args & C)
      return 3;
   else if (args & B)
      return 2;
   else if (args & A)
      return 1;
   else
      return 0;
}

enum gc_op_type
gc_op_type(enum gc_op op)
{
   switch (op) {
   case GC_TEXLD:
   case GC_TEXLDB:
   case GC_TEXLDD:
   case GC_TEXLDL:
      return GC_OP_TYPE_SAMPLER;

   case GC_BRANCH:
      return GC_OP_TYPE_BRANCH;

   default:
      return GC_OP_TYPE_ALU;
   }
}

#define SWIZ(s0, s1, s2)   \
   (s0 & 0x3) |            \
   ((s1 & 0x3) << 2) |     \
   ((s2 & 0x3) << 4)

static const uint8_t op_src_swizzle[] = {
   [GC_ADD] = SWIZ(0,  2, -1),
   [GC_MAD] = SWIZ(0,  1,  2),
   [GC_MUL] = SWIZ(0,  1,  2),
   [GC_DST] = SWIZ(0,  1, -1),
   [GC_DP3] = SWIZ(0,  1, -1),
   [GC_DP4] = SWIZ(0,  1, -1),
   [GC_DSX] = SWIZ(0, -1, -1) | SWIZ_REP_SRC0_TO_SRC2,
   [GC_DSY] = SWIZ(0, -1, -1) | SWIZ_REP_SRC0_TO_SRC2,
   [GC_MOV] = SWIZ(2, -1, -1),
   [GC_MOVAR] = SWIZ(2, -1, -1),
   [GC_RCP] = SWIZ(2, -1, -1),
   [GC_RSQ] = SWIZ(2, -1, -1),
   [GC_SELECT] = SWIZ(0,  1,  2),
   [GC_SET] = SWIZ(0,  1, -1),
   [GC_EXP] = SWIZ(2, -1, -1),
   [GC_LOG] = SWIZ(2, -1, -1),
   [GC_FRC] = SWIZ(2, -1, -1),
   [GC_BRANCH] = SWIZ(0,  1, -1),
   [GC_TEXKILL_IF] = SWIZ(0, 1, -1),
   [GC_SQRT] = SWIZ(2, -1, -1),
   [GC_SIN] = SWIZ(2, -1, -1),
   [GC_COS] = SWIZ(2, -1, -1),
   [GC_FLOOR] = SWIZ(2, -1, -1),
   [GC_CEIL] = SWIZ(2, -1, -1),
   [GC_TEXLD] = SWIZ(0, 1, -1),
};

unsigned
gc_op_src_swizzle(enum gc_op op)
{
   assert(gc_op_num_src(op));
   assert(op < ARRAY_SIZE(op_src_swizzle));
   assert(op_src_swizzle[op]);

   return op_src_swizzle[op];
}
