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
#include "isa.xml.h"

#include <string.h>

#include "util/macros.h"

struct instr {
   /* dword0: */
   uint32_t opc         : 6;
   uint32_t cond        : 5;
   uint32_t sat         : 1;
   uint32_t dst_use     : 1;
   uint32_t dst_amode   : 3;
   uint32_t dst_reg     : 7;
   uint32_t dst_comps   : 4;
   uint32_t tex_id      : 5;

   /* dword1: */
   uint32_t tex_amode   : 3;
   uint32_t tex_swiz    : 8;
   uint32_t src0_use    : 1;
   uint32_t src0_reg    : 9;
   uint32_t type_bit2   : 1;
   uint32_t src0_swiz   : 8;
   uint32_t src0_neg    : 1;
   uint32_t src0_abs    : 1;

   /* dword2: */
   uint32_t src0_amode  : 3;
   uint32_t src0_rgroup : 3;
   uint32_t src1_use    : 1;
   uint32_t src1_reg    : 9;
   uint32_t opcode_bit6 : 1;
   uint32_t src1_swiz   : 8;
   uint32_t src1_neg    : 1;
   uint32_t src1_abs    : 1;
   uint32_t src1_amode  : 3;
   uint32_t type_bit01  : 2;

   /* dword3: */
   union {
      struct {
         uint32_t src1_rgroup : 3;
         uint32_t src2_use    : 1;
         uint32_t src2_reg    : 9;
         uint32_t unk3_13     : 1;
         uint32_t src2_swiz   : 8;
         uint32_t src2_neg    : 1;
         uint32_t src2_abs    : 1;
         uint32_t unk3_24     : 1;
         uint32_t src2_amode  : 3;
         uint32_t src2_rgroup : 3;
         uint32_t unk3_31     : 1;
      };
      uint32_t dword3;
   };
};

static unsigned 
gc_pack_op(enum gc_op op)
{
#define OP_ENUM(op)              \
   case GC_##op:                 \
      return INST_OPCODE_##op;

   switch (op) {
   OP_ENUM(NOP)
   OP_ENUM(ADD)
   OP_ENUM(MAD)
   OP_ENUM(MUL)
   OP_ENUM(DST)
   OP_ENUM(DP3)
   OP_ENUM(DP4)
   OP_ENUM(DSX)
   OP_ENUM(DSY)
   OP_ENUM(MOV)
   OP_ENUM(MOVAR)
   OP_ENUM(MOVAF)
   OP_ENUM(RCP)
   OP_ENUM(RSQ)
   OP_ENUM(LITP)
   OP_ENUM(SELECT)
   OP_ENUM(SET)
   OP_ENUM(EXP)
   OP_ENUM(LOG)
   OP_ENUM(FRC)
   OP_ENUM(CALL)
   OP_ENUM(RET)
   OP_ENUM(BRANCH)
   OP_ENUM(TEXKILL)
   OP_ENUM(TEXLD)
   OP_ENUM(TEXLDB)
   OP_ENUM(TEXLDD)
   OP_ENUM(TEXLDL)
   OP_ENUM(TEXLDPCF)
   OP_ENUM(REP)
   OP_ENUM(ENDREP)
   OP_ENUM(LOOP)
   OP_ENUM(ENDLOOP)
   OP_ENUM(SQRT)
   OP_ENUM(SIN)
   OP_ENUM(COS)
   OP_ENUM(FLOOR)
   OP_ENUM(CEIL)
   OP_ENUM(SIGN)
   OP_ENUM(ADDLO)
   OP_ENUM(MULLO)
   OP_ENUM(BARRIER)
   OP_ENUM(SWIZZLE)
   OP_ENUM(I2I)
   OP_ENUM(I2F)
   OP_ENUM(F2I)
   OP_ENUM(F2IRND)
   OP_ENUM(F2I7)
   OP_ENUM(CMP)
   OP_ENUM(LOAD)
   OP_ENUM(STORE)
   OP_ENUM(COPYSIGN)
   OP_ENUM(GETEXP)
   OP_ENUM(GETMANT)
   OP_ENUM(NAN)
   OP_ENUM(NEXTAFTER)
   OP_ENUM(ROUNDEVEN)
   OP_ENUM(ROUNDAWAY)
   OP_ENUM(IADDSAT)
   OP_ENUM(IMULLO0)
   OP_ENUM(IMULLO1)
   OP_ENUM(IMULLOSAT0)
   OP_ENUM(IMULLOSAT1)
   OP_ENUM(IMULHI0)
   OP_ENUM(IMULHI1)
   OP_ENUM(IMUL0)
   OP_ENUM(IMUL1)
   OP_ENUM(IDIV0)
   OP_ENUM(IDIV1)
   OP_ENUM(IDIV2)
   OP_ENUM(IDIV3)
   OP_ENUM(IMOD0)
   OP_ENUM(IMOD1)
   OP_ENUM(IMOD2)
   OP_ENUM(IMOD3)
   OP_ENUM(IMADLO0)
   OP_ENUM(IMADLO1)
   OP_ENUM(IMADLOSAT0)
   OP_ENUM(IMADLOSAT1)
   OP_ENUM(IMADHI0)
   OP_ENUM(IMADHI1)
   OP_ENUM(IMADHISAT0)
   OP_ENUM(IMADHISAT1)
   OP_ENUM(HALFADD)
   OP_ENUM(HALFADDINC)
   OP_ENUM(MOVAI)
   OP_ENUM(IABS)
   OP_ENUM(LEADZERO)
   OP_ENUM(LSHIFT)
   OP_ENUM(RSHIFT)
   OP_ENUM(ROTATE)
   OP_ENUM(OR)
   OP_ENUM(AND)
   OP_ENUM(XOR)
   OP_ENUM(NOT)
   OP_ENUM(BITSELECT)
   OP_ENUM(POPCOUNT)
   OP_ENUM(STOREB)
   OP_ENUM(RGB2YUV)
   OP_ENUM(DIV)
   OP_ENUM(ATOM_ADD)
   OP_ENUM(ATOM_XCHG)
   OP_ENUM(ATOM_CMP_XCHG)
   OP_ENUM(ATOM_MIN)
   OP_ENUM(ATOM_MAX)
   OP_ENUM(ATOM_OR)
   OP_ENUM(ATOM_AND)
   OP_ENUM(ATOM_XOR)
   OP_ENUM(BIT_REV)
   OP_ENUM(BYTE_REV)
   OP_ENUM(TEXLDLPCF)
   OP_ENUM(TEXLDGPCF)
   OP_ENUM(PACK)
   OP_ENUM(CONV)
   OP_ENUM(DP2)
   OP_ENUM(NORM_DP2)
   OP_ENUM(NORM_DP3)
   OP_ENUM(NORM_DP4)
   OP_ENUM(NORM_MUL)
   OP_ENUM(STORE_ATTR)
   OP_ENUM(LOAD_ATTR)
   OP_ENUM(EMIT)
   OP_ENUM(RESTART)
   OP_ENUM(NOP7C)
   OP_ENUM(NOP7D)
   OP_ENUM(NOP7E)
   OP_ENUM(NOP7F)

   default:
      unreachable("unsupported op code");
      break;
   }

#undef OP_ENUM
}

static enum gc_op
gc_unpack_op(unsigned op)
{
#define OP_ENUM(op)        \
   case INST_OPCODE_##op:  \
      return GC_##op;

   switch (op) {
   OP_ENUM(NOP)
   OP_ENUM(ADD)
   OP_ENUM(MAD)
   OP_ENUM(MUL)
   OP_ENUM(DST)
   OP_ENUM(DP3)
   OP_ENUM(DP4)
   OP_ENUM(DSX)
   OP_ENUM(DSY)
   OP_ENUM(MOV)
   OP_ENUM(MOVAR)
   OP_ENUM(MOVAF)
   OP_ENUM(RCP)
   OP_ENUM(RSQ)
   OP_ENUM(LITP)
   OP_ENUM(SELECT)
   OP_ENUM(SET)
   OP_ENUM(EXP)
   OP_ENUM(LOG)
   OP_ENUM(FRC)
   OP_ENUM(CALL)
   OP_ENUM(RET)
   OP_ENUM(BRANCH)
   OP_ENUM(TEXKILL)
   OP_ENUM(TEXLD)
   OP_ENUM(TEXLDB)
   OP_ENUM(TEXLDD)
   OP_ENUM(TEXLDL)
   OP_ENUM(TEXLDPCF)
   OP_ENUM(REP)
   OP_ENUM(ENDREP)
   OP_ENUM(LOOP)
   OP_ENUM(ENDLOOP)
   OP_ENUM(SQRT)
   OP_ENUM(SIN)
   OP_ENUM(COS)
   OP_ENUM(FLOOR)
   OP_ENUM(CEIL)
   OP_ENUM(SIGN)
   OP_ENUM(ADDLO)
   OP_ENUM(MULLO)
   OP_ENUM(BARRIER)
   OP_ENUM(SWIZZLE)
   OP_ENUM(I2I)
   OP_ENUM(I2F)
   OP_ENUM(F2I)
   OP_ENUM(F2IRND)
   OP_ENUM(F2I7)
   OP_ENUM(CMP)
   OP_ENUM(LOAD)
   OP_ENUM(STORE)
   OP_ENUM(COPYSIGN)
   OP_ENUM(GETEXP)
   OP_ENUM(GETMANT)
   OP_ENUM(NAN)
   OP_ENUM(NEXTAFTER)
   OP_ENUM(ROUNDEVEN)
   OP_ENUM(ROUNDAWAY)
   OP_ENUM(IADDSAT)
   OP_ENUM(IMULLO0)
   OP_ENUM(IMULLO1)
   OP_ENUM(IMULLOSAT0)
   OP_ENUM(IMULLOSAT1)
   OP_ENUM(IMULHI0)
   OP_ENUM(IMULHI1)
   OP_ENUM(IMUL0)
   OP_ENUM(IMUL1)
   OP_ENUM(IDIV0)
   OP_ENUM(IDIV1)
   OP_ENUM(IDIV2)
   OP_ENUM(IDIV3)
   OP_ENUM(IMOD0)
   OP_ENUM(IMOD1)
   OP_ENUM(IMOD2)
   OP_ENUM(IMOD3)
   OP_ENUM(IMADLO0)
   OP_ENUM(IMADLO1)
   OP_ENUM(IMADLOSAT0)
   OP_ENUM(IMADLOSAT1)
   OP_ENUM(IMADHI0)
   OP_ENUM(IMADHI1)
   OP_ENUM(IMADHISAT0)
   OP_ENUM(IMADHISAT1)
   OP_ENUM(HALFADD)
   OP_ENUM(HALFADDINC)
   OP_ENUM(MOVAI)
   OP_ENUM(IABS)
   OP_ENUM(LEADZERO)
   OP_ENUM(LSHIFT)
   OP_ENUM(RSHIFT)
   OP_ENUM(ROTATE)
   OP_ENUM(OR)
   OP_ENUM(AND)
   OP_ENUM(XOR)
   OP_ENUM(NOT)
   OP_ENUM(BITSELECT)
   OP_ENUM(POPCOUNT)
   OP_ENUM(STOREB)
   OP_ENUM(RGB2YUV)
   OP_ENUM(DIV)
   OP_ENUM(ATOM_ADD)
   OP_ENUM(ATOM_XCHG)
   OP_ENUM(ATOM_CMP_XCHG)
   OP_ENUM(ATOM_MIN)
   OP_ENUM(ATOM_MAX)
   OP_ENUM(ATOM_OR)
   OP_ENUM(ATOM_AND)
   OP_ENUM(ATOM_XOR)
   OP_ENUM(BIT_REV)
   OP_ENUM(BYTE_REV)
   OP_ENUM(TEXLDLPCF)
   OP_ENUM(TEXLDGPCF)
   OP_ENUM(PACK)
   OP_ENUM(CONV)
   OP_ENUM(DP2)
   OP_ENUM(NORM_DP2)
   OP_ENUM(NORM_DP3)
   OP_ENUM(NORM_DP4)
   OP_ENUM(NORM_MUL)
   OP_ENUM(STORE_ATTR)
   OP_ENUM(LOAD_ATTR)
   OP_ENUM(EMIT)
   OP_ENUM(RESTART)
   OP_ENUM(NOP7C)
   OP_ENUM(NOP7D)
   OP_ENUM(NOP7E)
   OP_ENUM(NOP7F)

   default:
      unreachable("unsupported op code");
      break;     
   }

#undef OP_ENUM
}

static unsigned
gc_pack_operand_type(enum gc_operand_type type)
{
#define TYPE_ENUM(type)       \
   case GC_OPERAND_##type:    \
      return INST_TYPE_##type;

   switch (type) {
   TYPE_ENUM(F32)
   TYPE_ENUM(S32)
   TYPE_ENUM(S8)
   TYPE_ENUM(U16)
   TYPE_ENUM(F16)
   TYPE_ENUM(S16)
   TYPE_ENUM(U32)
   TYPE_ENUM(U8)

   default:
      unreachable("unsupported operand type");
      break;
   }

#undef TYPE_ENUM
}

static enum gc_operand_type
gc_unpack_operand_type(unsigned type)
{
#define TYPE_ENUM(type)          \
   case INST_TYPE_##type:        \
      return GC_OPERAND_##type;

   switch (type) {
   TYPE_ENUM(F32)
   TYPE_ENUM(S32)
   TYPE_ENUM(S8)
   TYPE_ENUM(U16)
   TYPE_ENUM(F16)
   TYPE_ENUM(S16)
   TYPE_ENUM(U32)
   TYPE_ENUM(U8)
   default:
      unreachable("unsupported operand type");
      break;
   }

#undef TYPE_ENUM
}

static unsigned
gc_pack_cond(enum gc_cond condition)
{
#define COND_ENUM(op)            \
   case GC_COND_##op:            \
      return INST_CONDITION_##op;

   switch (condition) {
   COND_ENUM(TRUE)
   COND_ENUM(GT)
   COND_ENUM(LT)
   COND_ENUM(GE)
   COND_ENUM(LE)
   COND_ENUM(EQ)
   COND_ENUM(NE)
   COND_ENUM(AND)
   COND_ENUM(OR)
   COND_ENUM(XOR)
   COND_ENUM(NOT)
   COND_ENUM(NZ)
   COND_ENUM(GEZ)
   COND_ENUM(GZ)
   COND_ENUM(LEZ)
   COND_ENUM(LZ)

   default:
      unreachable("unsupported condition");
      break;
   }

#undef COND_ENUM
}

static enum gc_cond
gc_unpack_cond(unsigned condition)
{
#define COND_ENUM(op)         \
   case INST_CONDITION_##op:  \
      return GC_COND_##op;

   switch (condition) {
   COND_ENUM(TRUE)
   COND_ENUM(GT)
   COND_ENUM(LT)
   COND_ENUM(GE)
   COND_ENUM(LE)
   COND_ENUM(EQ)
   COND_ENUM(NE)
   COND_ENUM(AND)
   COND_ENUM(OR)
   COND_ENUM(XOR)
   COND_ENUM(NOT)
   COND_ENUM(NZ)
   COND_ENUM(GEZ)
   COND_ENUM(GZ)
   COND_ENUM(LEZ)
   COND_ENUM(LZ)

   default:
      unreachable("unsupported condition");
      break;
   }

#undef COND_ENUM
}

static unsigned
gc_pack_register_group(enum gc_register_group group)
{
   switch (group) {
   case GC_REGISTER_GROUP_INTERNAL:
      return INST_RGROUP_INTERNAL;

   case GC_REGISTER_GROUP_TEMP:
      return INST_RGROUP_TEMP;

   case GC_REGISTER_GROUP_UNIFORM:
      /* TODO: take register number into account */
      return INST_RGROUP_UNIFORM_1;

   default:
      unreachable("unsupported register group");
      break;
   }
}

static enum gc_register_group
gc_unpack_register_group(unsigned group)
{
   switch (group) {
   case INST_RGROUP_INTERNAL:
      return GC_REGISTER_GROUP_INTERNAL;

   case INST_RGROUP_TEMP:
      return GC_REGISTER_GROUP_TEMP;

   case INST_RGROUP_UNIFORM_0:
      /* fall-through */
   case INST_RGROUP_UNIFORM_1:
      return GC_REGISTER_GROUP_UNIFORM;

   default:
      unreachable("unsupported register group");
      break;
   }
}

static unsigned
gc_pack_addressing_mode( enum gc_addressing_mode mode)
{
   switch (mode)
   {
   case GC_ADDRESSING_MODE_DIRECT:
      return INST_AMODE_DIRECT;

   case GC_ADDRESSING_MODE_ADD_A_X:
      return INST_AMODE_ADD_A_X;

   case GC_ADDRESSING_MODE_ADD_A_Y:
      return INST_AMODE_ADD_A_Y;

   case GC_ADDRESSING_MODE_ADD_A_Z:
      return INST_AMODE_ADD_A_Z;

   case GC_ADDRESSING_MODE_ADD_A_W:
      return INST_AMODE_ADD_A_W;

   default:
      unreachable("unsupported addressing mode");
      break;
   }
}

static enum gc_addressing_mode
gc_unpack_addressing_mode(unsigned mode)
{
   switch (mode)
   {
   case INST_AMODE_DIRECT:
      return GC_ADDRESSING_MODE_DIRECT;

   case INST_AMODE_ADD_A_X:
      return GC_ADDRESSING_MODE_ADD_A_X;

   case INST_AMODE_ADD_A_Y:
      return GC_ADDRESSING_MODE_ADD_A_Y;

   case INST_AMODE_ADD_A_Z:
      return GC_ADDRESSING_MODE_ADD_A_Z;

   case INST_AMODE_ADD_A_W:
      return GC_ADDRESSING_MODE_ADD_A_W;

   default:
      unreachable("unsupported addressing mode");
      break;
   }
}

static void
gc_pack_dst(const struct gc_instr *instr, struct instr *bin)
{
   assert(instr->dst.reg <= 127);

   bin->dst_amode = gc_pack_addressing_mode(instr->dst.amode);
   bin->dst_comps = instr->dst.comps;
   bin->dst_reg = instr->dst.reg;
   bin->dst_use = 1;
}

static void
gc_unpack_dst(const struct instr *i, struct gc_instr *instr)
{
   instr->dst.reg = i->dst_reg;
   instr->dst.comps = i->dst_comps;
   instr->dst.amode = gc_unpack_addressing_mode(i->dst_amode);
}

static void
gc_pack_src(const struct gc_instr_src *src, int num, struct instr *bin)
{
   assert(src->reg <= 511);

   switch (num) {
   case 0:
      bin->src0_abs = src->abs;
      bin->src0_amode = gc_pack_addressing_mode(src->amode);
      bin->src0_neg = src->neg;
      bin->src0_reg = src->reg;
      bin->src0_rgroup = gc_pack_register_group(src->rgroup);
      bin->src0_swiz = src->swiz;
      bin->src0_use = src->use;
      break;

   case 1:
      bin->src1_abs = src->abs;
      bin->src1_amode = gc_pack_addressing_mode(src->amode);
      bin->src1_neg = src->neg;
      bin->src1_reg = src->reg;
      bin->src1_rgroup = gc_pack_register_group(src->rgroup);
      bin->src1_swiz = src->swiz;
      bin->src1_use = src->use;
      break;

   case 2:
      bin->src2_abs = src->abs;
      bin->src2_amode = gc_pack_addressing_mode(src->amode);
      bin->src2_neg = src->neg;
      bin->src2_reg = src->reg;
      bin->src2_rgroup = gc_pack_register_group(src->rgroup);
      bin->src2_swiz = src->swiz;
      bin->src2_use = src->use;
      break;

   default:
      unreachable("unsupported src index");
      break;
   }
}

static void
gc_unpack_src(const struct instr *i, int num, struct gc_instr_src *src)
{
   switch (num) {
   case 0:
      src->use = i->src0_use;
      if (src->use) {
         src->reg = i->src0_reg;
         src->swiz = i->src0_swiz;
         src->neg = i->src0_neg;
         src->abs = i->src0_abs;
         src->amode = gc_unpack_addressing_mode(i->src0_amode);
         src->rgroup = gc_unpack_register_group(i->src0_rgroup);
      }
      break;

   case 1:
      src->use = i->src1_use;
      if (src->use) {
         src->reg = i->src1_reg;
         src->swiz = i->src1_swiz;
         src->neg = i->src1_neg;
         src->abs = i->src1_abs;
         src->amode = gc_unpack_addressing_mode(i->src1_amode);
         src->rgroup = gc_unpack_register_group(i->src1_rgroup);
      }
      break;

   case 2:
      src->use = i->src2_use;
      if (src->use) {
         src->reg = i->src2_reg;
         src->swiz = i->src2_swiz;
         src->neg = i->src2_neg;
         src->abs = i->src2_abs;
         src->amode = gc_unpack_addressing_mode(i->src2_amode);
         src->rgroup = gc_unpack_register_group(i->src2_rgroup);
      }
      break;

   default:
      unreachable("unsupported src index");
      break;
   }
}

static void
gc_pack_alu(const struct gc_instr *instr, struct instr *bin)
{
   const struct gc_instr_alu *alu = &instr->alu;

   gc_pack_src(&alu->src[0], 0, bin);
   gc_pack_src(&alu->src[1], 1, bin);
   gc_pack_src(&alu->src[2], 2, bin);
}

static void
gc_unpack_alu(const struct instr *i, struct gc_instr *instr)
{
   struct gc_instr_alu *alu = &instr->alu;

   gc_unpack_src(i, 0, &alu->src[0]);
   gc_unpack_src(i, 1, &alu->src[1]);
   gc_unpack_src(i, 2, &alu->src[2]);
}

static void
gc_pack_branch(const struct gc_instr *instr, struct instr *bin)
{
   const struct gc_instr_branch *branch = &instr->branch;

   bin->dword3 = VIV_ISA_WORD_3_SRC2_IMM(branch->imm);
   gc_pack_src(&branch->src[0], 0, bin);
   gc_pack_src(&branch->src[1], 1, bin);
}

static void
gc_unpack_branch(const struct instr *i, struct gc_instr *instr)
{
   struct gc_instr_branch *branch = &instr->branch;
   int imm = (i->dword3 & VIV_ISA_WORD_3_SRC2_IMM__MASK)
              >> VIV_ISA_WORD_3_SRC2_IMM__SHIFT;

   branch->imm = imm;

   gc_unpack_src(i, 0, &branch->src[0]);
   gc_unpack_src(i, 1, &branch->src[1]);
}

static void
gc_pack_sampler(const struct gc_instr *instr, struct instr *bin)
{
   const struct gc_instr_sampler *sampler = &instr->sampler;

   bin->tex_amode = gc_pack_addressing_mode(sampler->amode);
   bin->tex_id = sampler->id;
   bin->tex_swiz = sampler->swiz;

   gc_pack_src(&sampler->src, 0, bin);
}

static void
gc_unpack_sampler(const struct instr *i, struct gc_instr *instr)
{
   struct gc_instr_sampler *sampler = &instr->sampler;

   sampler->amode = gc_unpack_addressing_mode(i->tex_amode);
   sampler->id = i->tex_id;
   sampler->swiz = i->tex_swiz;

   gc_unpack_src(i, 0, &sampler->src);
}

void
gc_pack_instr(const struct gc_instr *instr, uint32_t *packed_instr)
{
   assert(instr);
   assert(packed_instr);

   struct instr *bin = (struct instr *)packed_instr;
   memset(bin, 0, sizeof(*bin));

   const unsigned opc = gc_pack_op(instr->opcode);
   const unsigned type = gc_pack_operand_type(instr->operand_type);

   bin->cond = gc_pack_cond(instr->condition);
   bin->type_bit01 = type & 0x3;
   bin->type_bit2 = !!(type & 0x4);
   bin->opc = opc & 0x3f;
   bin->opcode_bit6 = !!(opc & 0x40);
   bin->sat = instr->saturate;

   if (gc_op_has_dst(instr->opcode))
      gc_pack_dst(instr, bin);

   switch (instr->type) {
   case GC_OP_TYPE_ALU:
      gc_pack_alu(instr, bin);
      break;
   case GC_OP_TYPE_BRANCH:
      gc_pack_branch(instr, bin);
      break;
   case GC_OP_TYPE_SAMPLER:
      gc_pack_sampler(instr, bin);
      break;
   }
}

void
gc_unpack_instr(const uint32_t *packed_instr, struct gc_instr *instr)
{
   assert(packed_instr);
   assert(instr);
   const struct instr *i = (struct instr *)packed_instr;

   memset(instr, 0, sizeof(*instr));

   instr->condition = gc_unpack_cond(i->cond);
   instr->operand_type = gc_unpack_operand_type(i->type_bit01 | (i->type_bit2 << 2));
   instr->opcode = gc_unpack_op(i->opc | (i->opcode_bit6 << 6));
   instr->saturate = i->sat;
   instr->type = gc_op_type(instr->opcode);

   if (i->dst_use) {
      assert(gc_op_has_dst(instr->opcode));
      gc_unpack_dst(i, instr);
   }

   switch (instr->type) {
   case GC_OP_TYPE_ALU:
      gc_unpack_alu(i, instr);
      break;
   case GC_OP_TYPE_BRANCH:
      gc_unpack_branch(i, instr);
      break;
   case GC_OP_TYPE_SAMPLER:
      gc_unpack_sampler(i, instr);
      break;
   };
}
