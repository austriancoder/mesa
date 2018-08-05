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

#ifndef H_GC_INSTR
#define H_GC_INSTR

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

enum gc_cond {
   GC_COND_TRUE,
   GC_COND_GT,
   GC_COND_LT,
   GC_COND_GE,
   GC_COND_LE,
   GC_COND_EQ,
   GC_COND_NE,
   GC_COND_AND,
   GC_COND_OR,
   GC_COND_XOR,
   GC_COND_NOT,
   GC_COND_NZ,
   GC_COND_GEZ,
   GC_COND_GZ,
   GC_COND_LEZ,
   GC_COND_LZ,
};

enum gc_op {
   GC_NOP,
   GC_ADD,
   GC_MAD,
   GC_MUL,
   GC_DST,
   GC_DP3,
   GC_DP4,
   GC_DSX,
   GC_DSY,
   GC_MOV,
   GC_MOVAR,
   GC_MOVAF,
   GC_RCP,
   GC_RSQ,
   GC_LITP,
   GC_SELECT,
   GC_SET,
   GC_EXP,
   GC_LOG,
   GC_FRC,
   GC_CALL,
   GC_RET,
   GC_BRANCH,
   GC_TEXKILL,
   GC_TEXKILL_IF,
   GC_TEXLD,
   GC_TEXLDB,
   GC_TEXLDD,
   GC_TEXLDL,
   GC_TEXLDPCF,
   GC_REP,
   GC_ENDREP,
   GC_LOOP,
   GC_ENDLOOP,
   GC_SQRT,
   GC_SIN,
   GC_COS,
   GC_FLOOR,
   GC_CEIL,
   GC_SIGN,
   GC_ADDLO,
   GC_MULLO,
   GC_BARRIER,
   GC_SWIZZLE,
   GC_I2I,
   GC_I2F,
   GC_F2I,
   GC_F2IRND,
   GC_F2I7,
   GC_CMP,
   GC_LOAD,
   GC_STORE,
   GC_COPYSIGN,
   GC_GETEXP,
   GC_GETMANT,
   GC_NAN,
   GC_NEXTAFTER,
   GC_ROUNDEVEN,
   GC_ROUNDAWAY,
   GC_IADDSAT,
   GC_IMULLO0,
   GC_IMULLO1,
   GC_IMULLOSAT0,
   GC_IMULLOSAT1,
   GC_IMULHI0,
   GC_IMULHI1,
   GC_IMUL0,
   GC_IMUL1,
   GC_IDIV0,
   GC_IDIV1,
   GC_IDIV2,
   GC_IDIV3,
   GC_IMOD0,
   GC_IMOD1,
   GC_IMOD2,
   GC_IMOD3,
   GC_IMADLO0,
   GC_IMADLO1,
   GC_IMADLOSAT0,
   GC_IMADLOSAT1,
   GC_IMADHI0,
   GC_IMADHI1,
   GC_IMADHISAT0,
   GC_IMADHISAT1,
   GC_HALFADD,
   GC_HALFADDINC,
   GC_MOVAI,
   GC_IABS,
   GC_LEADZERO,
   GC_LSHIFT,
   GC_RSHIFT,
   GC_ROTATE,
   GC_OR,
   GC_AND,
   GC_XOR,
   GC_NOT,
   GC_BITSELECT,
   GC_POPCOUNT,
   GC_STOREB,
   GC_RGB2YUV,
   GC_DIV,
   GC_ATOM_ADD,
   GC_ATOM_XCHG,
   GC_ATOM_CMP_XCHG,
   GC_ATOM_MIN,
   GC_ATOM_MAX,
   GC_ATOM_OR,
   GC_ATOM_AND,
   GC_ATOM_XOR,
   GC_BIT_REV,
   GC_BYTE_REV,
   GC_TEXLDLPCF,
   GC_TEXLDGPCF,
   GC_PACK,
   GC_CONV,
   GC_DP2,
   GC_NORM_DP2,
   GC_NORM_DP3,
   GC_NORM_DP4,
   GC_NORM_MUL,
   GC_STORE_ATTR,
   GC_LOAD_ATTR,
   GC_EMIT,
   GC_RESTART,
   GC_NOP7C,
   GC_NOP7D,
   GC_NOP7E,
   GC_NOP7F,
};

enum gc_op_type {
   GC_OP_TYPE_ALU = 0,
   GC_OP_TYPE_BRANCH,
   GC_OP_TYPE_SAMPLER,
};

enum gc_operand_type {
   GC_OPERAND_F32 = 0,
   GC_OPERAND_S32,
   GC_OPERAND_S8,
   GC_OPERAND_U16,
   GC_OPERAND_F16,
   GC_OPERAND_S16,
   GC_OPERAND_U32,
   GC_OPERAND_U8
};

enum gc_addressing_mode {
   GC_ADDRESSING_MODE_DIRECT = 0,
   GC_ADDRESSING_MODE_ADD_A_X,
   GC_ADDRESSING_MODE_ADD_A_Y,
   GC_ADDRESSING_MODE_ADD_A_Z,
   GC_ADDRESSING_MODE_ADD_A_W,
};

enum gc_register_group {
   GC_REGISTER_GROUP_TEMP = 0,
   GC_REGISTER_GROUP_INTERNAL,
   GC_REGISTER_GROUP_UNIFORM,
};

struct gc_instr_dst {
   enum gc_addressing_mode amode:3; /* INST_AMODE_* */
   unsigned comps:4;                /* INST_COMPS_* */

   /* after RA: register number 0..127 */
   unsigned reg;
};

struct gc_instr_src {
   unsigned use:1;                  /* 0: not in use, 1: in use */
   unsigned swiz:8;                 /* INST_SWIZ */
   unsigned neg:1;                  /* negate (flip sign) if set */
   unsigned abs:1;                  /* absolute (remove sign) if set */
   enum gc_addressing_mode amode:3; /* INST_AMODE_* */
   enum gc_register_group rgroup:3; /* INST_RGROUP_* */

   /* after RA: register or uniform number 0..511 */
   unsigned reg;
};

struct gc_instr_alu {
   struct gc_instr_src src[3];
};

struct gc_instr_branch {
   struct gc_instr_src src[2];
   unsigned imm;                    /* takes place of src[2] for BRANCH/CALL */
};

struct gc_instr_sampler {
   unsigned id:5;                   /* sampler id */
   enum gc_addressing_mode amode:3; /* INST_AMODE_* */
   unsigned swiz:8;                 /* INST_SWIZ */

   struct gc_instr_src src;
};

struct gc_instr {
   enum gc_op_type type;

   enum gc_op opcode;
   enum gc_operand_type operand_type;
   enum gc_cond condition;
   bool saturate;

   struct gc_instr_dst dst;

   union {
      struct gc_instr_alu alu;
      struct gc_instr_branch branch;
      struct gc_instr_sampler sampler;
   };
};

const char *
gc_op_name(enum gc_op op);

bool
gc_op_has_dst(enum gc_op op);

int
gc_op_num_src(enum gc_op op);

enum gc_op_type
gc_op_type(enum gc_op op);

#define BIT(bit) (1u << bit)
#define SWIZ_REP_SRC0_TO_SRC2 BIT(7)

unsigned
gc_op_src_swizzle(enum gc_op op);

void
gc_pack_instr(const struct gc_instr *instr, uint32_t *packed_instr);

void
gc_unpack_instr(const uint32_t *packed_instr, struct gc_instr *instr);


#include "vivante/gc/isa.xml.h"

/* Broadcast swizzle to all four components */
#define INST_SWIZ_BROADCAST(x) \
        (INST_SWIZ_X(x) | INST_SWIZ_Y(x) | INST_SWIZ_Z(x) | INST_SWIZ_W(x))
/* Identity (NOP) swizzle */
#define INST_SWIZ_IDENTITY \
        (INST_SWIZ_X(0) | INST_SWIZ_Y(1) | INST_SWIZ_Z(2) | INST_SWIZ_W(3))
/* Fully specified swizzle */
#define INST_SWIZ(x, y, z, w) \
        (INST_SWIZ_X(x) | INST_SWIZ_Y(y) | INST_SWIZ_Z(z) | INST_SWIZ_W(w))

#ifdef __cplusplus
}
#endif

#endif // H_GC_INSTR
