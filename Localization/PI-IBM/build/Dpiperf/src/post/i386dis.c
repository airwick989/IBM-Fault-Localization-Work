/*   IBM Performance Inspector
 *   Copyright (c) International Business Machines Corp., 2003 - 2009
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/*
 * Intel 386/486/Pentium Disassember
 *   Exported Function:
 *      i386_instruction       - Disassembles 1 Intel 386 instruction
 *   Static Functions:
 *      rm_mod_byte_present    - Detects RM/mod byte
 *      operand_value          - Finds operand value and pointer
 *      get_instruction_length - Finds instruction length
 *      set_prefix_value       - Sets prefix value
 *      clear_prefix_values    - Resets prefix values
 *      decode_instruction     - Disasms after Prefixes & Subtables
 */

#include <stdio.h>
#include <string.h>
#include "itypes.h"
#include "a2n.h"
#include "post.h"

#ifndef I64LL
   #if defined(_WINDOWS) || defined(WINDOWS) || defined(WIN32) || defined(_WIN32)
      #define I64LL       "I64"
   #else
      #define I64LL       "ll"
   #endif
#endif

char OPNM[32];                          /* global inst type in ascii */

// use  noStopOnInvalid(void)  to istop = 0
int istop  = 1;                         /* invalid-stop 1 :: def: continue */

#if defined(USE_LIBOPCODES)

   #include "dis-asm.h"

unsigned long disassemble( char *target, char *insbuf, bfd_vma ip, disassembler_ftype func );

/************************/
char * i386_instruction( char * data,   /* instruction    */
                         uint64 addr,   /* address/offset */
                         int  * size,
                         char * buf,
                         int    mode )  /* 0 = 16, 2 = 32, 1 = 64-bit segment */
{

#if defined(CONFIG_S390) || defined(CONFIG_S390X)
   *size = disassemble( buf, data, (bfd_vma)addr, print_insn_s390 );
#else
   *size = disassemble( buf, data, (bfd_vma)addr, print_insn_i386_intel );
#endif

   strcpy(OPNM, buf);
   return(OPNM);
}

#else

void panic(char * why);

/* Operand Format Display */
char * fdis[32] =
{
   "",
   "F_H",  "F_H44",  "F_H88", "F_H48", "F_D",  "F_S",  "F_SD", "F_SDP",
   "F_R",  "F_DR",   "F_RR",  "F_DRR", "F_HA", "F_SA", "F_SH", "F_HR",
   "F_H4", "F_HREL", "F_SH4"
};

/* Misscellaneous Display */
char * mdis[256] =
{
   /* Registers 0, 1-30 */
   "",                                                      //  0        not used
   "AL",  "CL",  "DL",  "BL",  "AH",  "CH",  "DH",  "BH",   //  1 -  8:  8-bit GPRs
   "AX",  "CX",  "DX",  "BX",  "SP",  "BP",  "SI",  "DI",   // 09 - 16: 16-bit GPRs
   "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI",  // 17 - 24: 32-bit GPRs
   "ES",  "CS",  "SS",  "DS",  "FS",  "GS",                 // 25 - 30: Segment registers

   /* 31-33 */
   "ST", "ST_NUM", "THREE",                                 // 31 - 33

   /* empty 34-39 */
   "",  "",  "",  "",  "",  "",                             // 34 - 39

   /* Registers based on operand size 40-47 */
   "eAX", "eCX", "eDX", "eBX", "eSP", "eBP", "eSI", "eDI",  // 40 - 47  (ex. AX | EAX | RAX)

   /* Operand Size prefix(p_os) */
   "OS",                                                    // 48

   /* Address Size prefix(p_as) */
   "AS",                                                    // 49

   /* Addressing Methods 50-61 */
   "AM_A", "AM_C", "AM_D", "AM_E", "AM_G", "AM_I", "AM_J",  // 50 - 56
   "AM_M", "AM_O", "AM_R", "AM_S", "AM_T", "AM_F", "AM_P",  // 57 - 63
   "AM_Q", "AM_V", "AM_W", "AM_X", "AM_Y",                  // 64 - 68
   "",  "",  "",  "",  "",  "",  "",  "",  "",  "",  "",    // 69 - 79

   /* Operand Types  80-94 */
   "OT_A", "OT_B", "OT_C", "OT_D", "OT_P", "OT_Q", "OT_S",  // 80 - 86
   "OT_V", "OT_W",                                          // 87 - 88
   "OT_SI", "OT_LI", "OT_DQ" "OT_PI", "OT_PS", "OT_SS",     // 89 - 94
};

   #define FALSE              0
   #define TRUE               1
   #define LNULL ((void *) 0)
   #define INVALID_INSTRUCTION     "  *** invalid instruction ***"
   #define MAX_OPERANDS       3    /* maximum number of operands            */

/* little Endian input on little & big Endian machine */
   #ifdef _AIX
      #define WORD(n) ( ( (*(short int *)n) & 0xff) << 8) |  \
      ( ( (*(short int *)n >> 8) & 0xff) )
      #define DWORD(n) ( ( (*(int *)n) & 0xff) << 24) | \
      ( ( (*(int *)n >> 8) & 0xff) << 16) | \
      ( ( (*(int *)n >> 16) & 0xff) <<  8) | \
      (((*(int *)n >> 24) & 0xff))
      #define LKCONV
   #endif

   #ifdef _WIN32
      #define WORD(n) (*(short int *)n)
      #define DWORD(n) (*(int *)n)
      #define LKCONV
   #endif

   #ifdef _LINUX
      #define WORD(n) (*(short int *)n)
      #define DWORD(n) (*(int *)n)
      #define LKCONV
   #endif

char * i386_instruction (char *, uint64, int *, char *, int);

typedef struct
{
   int meth;                            /* address method */
   int type;                            /* operand type   */
} OPERAND;

typedef struct
{
   int       type;                      /*  type                 */
   char    * mnemonic;                  /* mnemonic              */
   OPERAND   par[MAX_OPERANDS];         /* operands              */
   int       mask;                      /* subtable mask or      */
                                        /* opsize mnemonic index */
   int       length;                    /* subtable size         */
} INSTRUCTION;

typedef struct
{
   int  p_valid;                        /* prefix count */

   BOOL p_es;
   BOOL p_cs;
   BOOL p_ss;
   BOOL p_ds;
   BOOL p_fs;
   BOOL p_gs;
   BOOL p_os;                           // Operand size
   BOOL p_as;                           // Address size
   BOOL p_lock;
   BOOL p_repne;
   BOOL p_rep;

   BOOL p_rex;                          //STJ
   int  p_rexType;                      // Type of REX.? prefix //STJ

} PREFIX_T;

int pre_tot[256][2] = {0};              /* prefix cnts. 0=>1, 1=>0 */

   #define SEG_PREFIX(p) ((p).p_es ? 1 : (p).p_cs ? 2 : (p).p_ss ? 3 : (p).p_ds \
                                ? 4 : (p).p_fs ? 5 : (p).p_gs ? 6 : 0)

/* Operand Format */
   #define F_H       1   /* %x                        */
   #define F_H44     2   /* %4x:%4x                   */
   #define F_H88     3   /* %8x:%8x                   */
   #define F_H48     4   /* %4x:%8x                   */
   #define F_D       5   /* %d                        */
   #define F_S       6   /* %s                        */
   #define F_SD      7   /* %s %d                     */
   #define F_SDP     8   /* %s[%d]                    */
   #define F_R       9   /* [reg]                     */
   #define F_DR     10   /* %d[reg] e.g 12[eax]       */
   #define F_RR     11   /* [reg][reg]                */
   #define F_DRR    12   /* [reg][reg*%d]             */
   #define F_HA     13   /* %x OR %s(symbol name)     */
   #define F_SA     14   /* %4x OR %s(symbol name)    */
   #define F_SH     15   /* %s%d                      */
   #define F_HR     16   /* %x[reg] e.g. 0x12[eax]    */
   #define F_H4     17   /* %4x                       */
   #define F_HREL   18   /* %x + relocation           */
   #define F_SH4    19   /* %s %4x                    */

/* op_type values */
   #define OPC_MASK   0x00FF  /* opcode field            */
   #define VALID      0x0100  /* valid opcode            */
   #define INVALID    0x0200  /* invalid opcode          */
   #define SUBTB      0x0400  /* Sub-table w/next byte for multi-byte opcodes */
   #define PREFIX     0x0800  /* prefix type             */
   #define OPSIZE     0x1000  /* depends on operand size */
   #define ADSIZE     0x2000  /* depends on address size */
   #define REXPRE     0x4000  /* REX prefix (64-bit mode)*/ //STJ
   #define SUBTB11B   0x8000  /* Sub-table w/mod = 11B   */ //STJ

/* op_type strings */
char * optypestr[8] =
{
   "VALID", "INVALID", "SUBTB", "PREFIX", "OPSIZE", "ADSIZE", "REXPRE", "SUBTB11B"
};

/* REX.??? prefix strings */
char * rexprefix[16] =
{
   " REX",    " REX.B",   " REX.X",   " REX.XB",
   " REX.R",  " REX.RB",  " REX.RX",  " REX.RXB",
   " REX.W",  " REX.WB",  " REX.WX",  " REX.WXB",
   " REX.WR", " REX.WRB", " REX.WRX", " REX.WRXB"
};

/* Registers    */
   #undef  AL
   #undef  BL
   #undef  CL
   #undef  DL
   #undef  AH
   #undef  BH
   #undef  CH
   #undef  DH
   #undef  AX
   #undef  BX
   #undef  CX
   #undef  DX
   #undef  EAX
   #undef  EBX
   #undef  ECX
   #undef  EDX
   #undef  SP
   #undef  BP
   #undef  SI
   #undef  DI
   #undef  ESP
   #undef  EBP
   #undef  ESI
   #undef  EDI
   #undef  CS
   #undef  DS
   #undef  ES
   #undef  FS
   #undef  GS
   #undef  SS

/* 8-bit general registers */
   #define AL      1
   #define CL      2
   #define DL      3
   #define BL      4
   #define AH      5
   #define CH      6
   #define DH      7
   #define BH      8
/* 16-bit general registers */
   #define AX      9
   #define CX      10
   #define DX      11
   #define BX      12
   #define SP      13
   #define BP      14
   #define SI      15
   #define DI      16
/* 32-bit general registers */
   #define EAX     17
   #define ECX     18
   #define EDX     19
   #define EBX     20
   #define ESP     21
   #define EBP     22
   #define ESI     23
   #define EDI     24
/* 16-bit segment registers */
   #define ES      25
   #define CS      26
   #define SS      27
   #define DS      28
   #define FS      29
   #define GS      30

   #define ST      31       /* stack top                     */
   #define ST_NUM  32       /* n-th stack elem  0 <= n <=7   */
   #define THREE   33       /* interrupt number 3            */
   #define MAX_REG_NUM  33

/* registers depending on operand size */
   #define eAX  40   /* AX | EAX  */
   #define eCX  41   /* CX | ECX  */
   #define eDX  42   /* DX | EDX  */
   #define eBX  43   /* BX | EBX  */

   #define eSP  44   /* SP | ESP  */
   #define eBP  45   /* BP | EBP  */
   #define eSI  46   /* SI | ESI  */
   #define eDI  47   /* DI | EDI  */

   #define OS   48   /* Operand Size prefix(p_os) */
   #define AS   49   /* Address Size prefix(p_as) */

/* Addressing Methods */
   #define AM_A  50  /* Direct address                                  */
   #define AM_C  51  /* modR/W[reg]  -> control reg                     */
   #define AM_D  52  /* modR/W[reg]  -> debug reg                       */
   #define AM_E  53  /* modR/W       -> general reg or memory           */
/* Memory: segment register + base, index,         */
/* scale factor, disp(on right with sign-extend)   */
   #define AM_G  54  /* modR/M[reg]  -> general reg.                    */
   #define AM_I  55  /* Immediate data                                  */
   #define AM_J  56  /* Instruction ptr reg + offset(32/48)             */
   #define AM_M  57  /* modR/M       -> memory(like AM_E)               */
   #define AM_O  58  /* no modR/M byte; operand offset(16/32)(p_as)     */
   #define AM_R  59  /* modR/M[reg]  -> general reg                     */
   #define AM_S  60  /* modR/W[reg]  -> segment reg                     */
   #define AM_T  61  /* modR/W[reg]  -> test register                   */
/* new AM's */
   #define AM_F  62  /* EFLAGS  reg */
   #define AM_P  63  /* reg of ModR/M => packed quad MMX reg */
   #define AM_Q  64  /* Operand: Reg => MMX, Mem => (see AM_E) */
   #define AM_V  65  /* reg of ModR/M => 128 XMM reg */
   #define AM_W  66  /* Operand: Reg => 128 XMM reg, Mem => (see AM_E) */
   #define AM_X  67  /* Mem via DS:SI */
   #define AM_Y  68  /* Mem via ES:DI */

/* Operand Types */
   #define OT_A  80  /* 2 16/32 bit operands(p_os)(BOUND) */
   #define OT_B  81  /* byte                              */
   #define OT_C  82  /* byte or word(p_os)                */
   #define OT_D  83  /* double word                       */
   #define OT_P  84  /* 32 or 48-bit pointer(p_os)        */
   #define OT_Q  85  /* quad word                         */
   #define OT_S  86  /* 6-byte pseudo-descriptor          */
   #define OT_V  87  /* word or double word(p_os)         */
   #define OT_W  88  /* word                              */
   #define OT_SI 89  /* short integer - 4 bytes           */
   #define OT_LI 90  /* long integer - 8 bytes            */
/* added types */
   #define OT_DP 91  /* Double Quad word       */
   #define OT_DQ QT_DP
   #define OT_PI 92  /* QuadWord mmx register  */
   #define OT_PS 93  /* 128 bit packed single-prec flt pt data  */
   #define OT_SS 94  /* scalar elem of a 128b 128b pkSngPrec fp  */



   #define MIN_OT   OT_A
   #define OT_NUM   (OT_SS - OT_A + 1)

/* Instruction Field Masks */
   #define BASE_MASK    0x07
   #define BYTE_MASK    0xff
   #define INDEX_MASK   0x38
   #define MOD_MASK     0xc0
   #define OC_MASK      0x38
   #define REG_MASK     0x38
   #define RM_MASK      0x07
   #define SS_MASK      0xc0

/* R/Mmod fields */
   #define V_MOD(b)     (((b) >> 6) & 3)
   #define V_REG(b)     (((b) >> 3) & 7)
   #define V_RM(b)      ((b) & 7)

/* SIB fields */
   #define V_SS(b)      (((b) >> 6) & 3)
   #define V_INDEX(b)   (((b) >> 3) & 7)
   #define V_BASE(b)    ((b) & 7)

   #define REG_TYPE(r)  (((r) == 1) ? 0 : (((r) == 2) ? 1 : 2))


//#EMP# This table isn't used, just for documentation purposes for now
//----------------------------------------------------------------------
// Definitions of all three-byte instructions (starting with 0x0F38)
// See Table A-4 in Appendix 2 of the "Intel 64 and IA-32 Architectures
// Software Developer's Manual", Volume 2B: Instruction Set Reference, N-Z
//----------------------------------------------------------------------
static INSTRUCTION i386_instruction_tbl_3b_0F38[] =
{
   /* 0x00 */ {VALID,     "pshufb",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3800
   /* 0x01 */ {VALID,     "phaddw",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3801
   /* 0x02 */ {VALID,     "phaddd",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3802
   /* 0x03 */ {VALID,    "phaddsw",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3803
   /* 0x04 */ {VALID,  "pmaddubsd",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3804
   /* 0x05 */ {VALID,     "phsubw",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3805
   /* 0x06 */ {VALID,     "phsubd",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3806
   /* 0x07 */ {VALID,    "phsubsw",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3807
   /* 0x08 */ {VALID,     "psignb",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3808
   /* 0x09 */ {VALID,     "psignw",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F3809
   /* 0x0A */ {VALID,     "psignd",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F380A
   /* 0x0B */ {VALID,   "pmulhrsw",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F380B
   /* 0x0C */ {INVALID},
   /* 0x0D */ {INVALID},
   /* 0x0E */ {INVALID},
   /* 0x0F */ {INVALID},

   /* 0x10 */ {INVALID},
   /* 0x11 */ {INVALID},
   /* 0x12 */ {INVALID},
   /* 0x13 */ {INVALID},
   /* 0x14 */ {INVALID},
   /* 0x15 */ {INVALID},
   /* 0x16 */ {INVALID},
   /* 0x17 */ {INVALID},
   /* 0x18 */ {INVALID},
   /* 0x19 */ {INVALID},
   /* 0x1A */ {INVALID},
   /* 0x1B */ {INVALID},
   /* 0x1C */ {VALID,      "pabsb",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F381C
   /* 0x1D */ {VALID,      "pabsw",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F381D
   /* 0x1E */ {VALID,      "pabsd",  AM_P, OT_Q, AM_Q, OT_Q},  // 0F381E
   /* 0x1F */ {INVALID},
};


//#EMP# This table isn't used, just for documentation purposes for now
//----------------------------------------------------------------------
// Definitions of all three-byte instructions (starting with 0x0F3A)
// See Table A-5 in Appendix 2 of the "Intel 64 and IA-32 Architectures
// Software Developer's Manual", Volume 2B: Instruction Set Reference, N-Z
//----------------------------------------------------------------------
static INSTRUCTION i386_instruction_tbl_3b_0F3A[] =
{
   /* 0x00 */ {INVALID},
   /* 0x01 */ {INVALID},
   /* 0x02 */ {INVALID},
   /* 0x03 */ {INVALID},
   /* 0x04 */ {INVALID},
   /* 0x05 */ {INVALID},
   /* 0x06 */ {INVALID},
   /* 0x07 */ {INVALID},
   /* 0x08 */ {INVALID},
   /* 0x09 */ {INVALID},
   /* 0x0A */ {INVALID},
   /* 0x0B */ {INVALID},
   /* 0x0C */ {INVALID},
   /* 0x0D */ {INVALID},
   /* 0x0E */ {INVALID},
   /* 0x0F */ {VALID,    "palignr",  AM_V, OT_Q, AM_Q, OT_Q, AM_I, OT_B},  // 0F3AFF
};


static INSTRUCTION g1_t[] =
{
   {0x00 | VALID,   "add"},
   {0x08 | VALID,   "or"},
   {0x10 | VALID,   "adc"},
   {0x18 | VALID,   "sbb"},
   {0x20 | VALID,   "and"},
   {0x28 | VALID,   "sub"},
   {0x30 | VALID,   "xor"},
   {0x38 | VALID,   "cmp"}
};

static INSTRUCTION g2_t[] =
{
   {0x00 | VALID,   "rol"},
   {0x08 | VALID,   "ror"},
   {0x10 | VALID,   "rcl"},
   {0x18 | VALID,   "rcr"},
   {0x20 | VALID,   "shl/sal"},         //STJ
   {0x28 | VALID,   "shr"},
   {0x30 | VALID,   "shl/sal"},         //STJ
   {0x38 | VALID,   "sar"}
};

static INSTRUCTION g3a_t[] =
{
   {0x00 | VALID,   "test", 0, 0, AM_I, OT_B},
   {0x10 | VALID,   "not"},
   {0x18 | VALID,   "neg"},
   {0x20 | VALID,   "mul",  0, 0, AL},
   {0x28 | VALID,   "imul", 0, 0, AL},
   {0x30 | VALID,   "div",  0, 0, AL},
   {0x38 | VALID,   "idiv", 0, 0, AL}
};

static INSTRUCTION g3b_t[] =
{
   {0x00 | VALID,   "test", 0, 0, AM_I, OT_V, 0},
   {0x10 | VALID,   "not",  0},
   {0x18 | VALID,   "neg",  0},
   {0x20 | VALID,   "mul",  0, 0, eAX},
   {0x28 | VALID,  "imul",  0, 0, eAX},
   {0x30 | VALID,   "div",  0, 0, eAX},
   {0x38 | VALID,  "idiv",  0, 0, eAX}
};

static INSTRUCTION g4_t[] =
{
   {0x00 | VALID,   "inc", AM_E, OT_B},
   {0x08 | VALID,   "dec", AM_E, OT_B}
};

static INSTRUCTION g5_t[] =
{
   {0x00 | VALID,    "inc", AM_E, OT_V},
   {0x08 | VALID,    "dec", AM_E, OT_V},
   {0x10 | VALID,   "call", AM_E, OT_V},
   {0x18 | VALID,   "call", AM_E, OT_P},
   {0x20 | VALID,   "jmp near", AM_E, OT_V},
   {0x28 | VALID,   "jmp far", AM_E, OT_P},
   {0x30 | VALID,   "push", AM_E, OT_V}
};

static INSTRUCTION g6_t[] =
{
   {0x00 | VALID,   "sldt", AM_E, OT_W},
   {0x08 | VALID,    "str", AM_E, OT_W},
   {0x10 | VALID,   "lldt", AM_E, OT_W},
   {0x18 | VALID,    "ltr", AM_E, OT_W},
   {0x20 | VALID,   "verr", AM_E, OT_W},
   {0x28 | VALID,   "verw", AM_E, OT_W}
};

static INSTRUCTION g7_t[] =
{
   {0x00 | VALID,   "sgdt", AM_M, OT_S},
   {0x08 | VALID,   "sidt", AM_M, OT_S},
   {0x10 | VALID,   "lgdt", AM_M, OT_S},
   {0x18 | VALID,   "lidt", AM_M, OT_S},
   {0x20 | VALID,   "smsw", AM_E, OT_W},
   {0x30 | VALID,   "lmsw", AM_E, OT_W},
   {0x38 | VALID, "invlpg", AM_E, OT_B}
};

static INSTRUCTION g8_t[] =
{
   {0x20 | VALID,    "bt"},
   {0x28 | VALID,   "bts"},
   {0x30 | VALID,   "btr"},
   {0x38 | VALID,   "btc"}
};

static INSTRUCTION g9_t[] =
{
   {0x08 | VALID,   "cmpxchg8b", AM_M, OT_Q},                // #EMP# Can also be: cmpxchg16b  AM_M, OT_DQ
   {0x30 | VALID,   "vmptrld",   AM_M, OT_Q},                // #EMP# Can also be: vmclear/vmxmon  AM_M, OT_Q
   {0x38 | VALID,   "vmptrst",   AM_M, OT_Q},                // #EMP#
};

static INSTRUCTION g12_t[] =
{
   {0x10 | VALID,   "psrlw", AM_P, OT_Q, AM_I, OT_B},
   {0x20 | VALID,   "psraw", AM_P, OT_Q, AM_I, OT_B},
   {0x30 | VALID,   "psllw", AM_P, OT_Q, AM_I, OT_B}
};

static INSTRUCTION g13_t[] =
{
   {0x10 | VALID,   "psrld", AM_P, OT_Q, AM_I, OT_B},
   {0x20 | VALID,   "psrad", AM_P, OT_Q, AM_I, OT_B},
   {0x30 | VALID,   "pslld", AM_P, OT_Q, AM_I, OT_B}
};

static INSTRUCTION g14_t[] =
{
   {0x10 | VALID,   "psrlq", AM_P, OT_Q, AM_I, OT_B},
   {0x18 | VALID,   "psrldq", AM_P, OT_Q, AM_I, OT_B},       // #EMP#
   {0x30 | VALID,   "psllq", AM_P, OT_Q, AM_I, OT_B},
   {0x38 | VALID,   "pslldq", AM_P, OT_Q, AM_I, OT_B}        // #EMP#
};

/* bit 543 of 76543210 */
static INSTRUCTION g15_t[] =
{
   {0x00 | VALID,  "fxsave",},
   {0x08 | VALID,  "fxrstor"},
   {0x10 | VALID,  "ldmxcsr"},
   {0x18 | VALID,  "stmxcsr"},
   {0x28 | VALID,  "lfence"},           //STJ
   {0x30 | VALID,  "mfence"},           //STJ
   {0x38 | VALID,  "sfence"}            //FIX ME, can also be CLFLUSH
};

static INSTRUCTION g16_t[] =
{
   {0x00 | VALID,    "prefetchnta", AM_E, OT_V},
   {0x08 | VALID,    "prefetcht0"},
   {0x10 | VALID,    "prefetcht1"},
   {0x18 | VALID,    "prefetcht2"}
};

//----------------------------------------------------------------------
// Definitions of all two-byte instructions (starting with 0F)
// See Table A-3 in Appendix 2 of the "Intel 64 and IA-32 Architectures
// Software Developer's Manual", Volume 2B: Instruction Set Reference, N-Z
//----------------------------------------------------------------------
static INSTRUCTION i386_instruction_tbl_2b[] =
{
   /* 0x00 */ {SUBTB, (char *)g6_t, 0, 0, 0, 0, 0, 0, OC_MASK, 6},
   /* 0x01 */ {SUBTB, (char *)g7_t, 0, 0, 0, 0, 0, 0, OC_MASK, 7},  // #EMP# Needs: vmcall, vmlaunch, vmresume, vmxoff, monitor, mwait, swapgs
   /* 0x02 */ {VALID,      "lar",    AM_G, OT_V, AM_E, OT_V},
   /* 0x03 */ {VALID,      "lsl",    AM_G, OT_V, AM_E, OT_V},
   /* 0x04 */ {INVALID},
   /* 0x05 */ {VALID,      "syscall"},  //STJ
   /* 0x06 */ {VALID,      "clts"},
   /* 0x07 */ {VALID,      "sysret"},   //STJ
   /* 0x08 */ {VALID,      "invd"},
   /* 0x09 */ {VALID,      "wbinvd"},
   /* 0x0a */ {INVALID},
   /* 0x0b */ {VALID,      "ud2"},
   /* 0x0c */ {INVALID},
   /* 0x0d */ {VALID,    "prefetch", AM_E, OT_V},
   /* 0x0e */ {INVALID},
   /* 0x0f */ {INVALID},

   /* 0x10 */ {VALID,      "movups", AM_V, OT_V, AM_E, OT_V},
   /* 0x11 */ {VALID,      "movups", AM_E, OT_V, AM_V, OT_V},
   /* 0x12 */ {VALID,      "movlps", AM_E, OT_V, AM_V, OT_V},
   /* 0x13 */ {VALID,      "movlps", AM_V, OT_V, AM_E, OT_V},
   /* 0x14 */ {VALID,    "unpcklps", AM_V, OT_V, AM_E, OT_V},
   /* 0x15 */ {VALID,    "unpckhps", AM_V, OT_V, AM_E, OT_V},
   /* 0x16 */ {VALID,      "movhps", AM_V, OT_V, AM_E, OT_V},
   /* 0x17 */ {VALID,      "movhps", AM_E, OT_V, AM_V, OT_V},
   /* 0x18 */ {SUBTB, (char *)g16_t, 0, 0, 0, 0, 0, 0, OC_MASK, 4},
   /* 0x19 */ {INVALID},
   /* 0x1a */ {INVALID},
   /* 0x1b */ {INVALID},
   /* 0x1c */ {INVALID},
   /* 0x1d */ {INVALID},
   /* 0x1e */ {INVALID},
   /* 0x1f */ {VALID,      "nop",    AM_E, OT_V},

   /* 0x20 */ {VALID,      "mov",    AM_R, OT_D, AM_C, OT_D},
   /* 0x21 */ {VALID,      "mov",    AM_R, OT_D, AM_D, OT_D},
   /* 0x22 */ {VALID,      "mov",    AM_C, OT_D, AM_R, OT_D},
   /* 0x23 */ {VALID,      "mov",    AM_D, OT_D, AM_R, OT_D},
   /* 0x24 */ {INVALID},                                            // #EMP# Was: {VALID,      "mov",    AM_R, OT_D, AM_T, OT_D},
   /* 0x25 */ {INVALID},
   /* 0x26 */ {INVALID},                                            // #EMP# Was: {VALID,      "mov",    AM_T, OT_D, AM_R, OT_D},
   /* 0x27 */ {INVALID},
   /* 0x28 */ {VALID,      "movaps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x29 */ {VALID,      "movaps",    AM_E, OT_A, AM_V, OT_V},
   /* 0x2a */ {VALID,      "cvtpi2ps",  AM_V, OT_V, AM_E, OT_V},
   /* 0x2b */ {VALID,      "movntps",   AM_E, OT_A, AM_V, OT_V},
   /* 0x2c */ {VALID,      "cvttps2pi", AM_E, OT_A, AM_G, OT_V},
   /* 0x2d */ {VALID,      "cvtps2pi",  AM_E, OT_A, AM_G, OT_V},
   /* 0x2e */ {VALID,      "ucomiss",   AM_E, OT_A, AM_V, OT_V},
   /* 0x2f */ {VALID,      "comiss",    AM_E, OT_A, AM_V, OT_V},

   /* 0x30 */ {VALID,      "wrmsr"},
   /* 0x31 */ {VALID,      "rdtsc"},
   /* 0x32 */ {VALID,      "rdmsr"},
   /* 0x33 */ {VALID,      "rdpmc"},
   /* 0x34 */ {VALID,      "sysenter"},
   /* 0x35 */ {VALID,      "sysexit"},
   /* 0x36 */ {INVALID},
   /* 0x37 */ {INVALID},
   /* 0x38 */ {INVALID},                                            // #EMP# 0F38: 3-byte opcode escape code
   /* 0x39 */ {INVALID},
   /* 0x3a */ {INVALID},                                            // #EMP# 0F3A: 3-byte opcode escape code
   /* 0x3b */ {INVALID},
   /* 0x3c */ {INVALID},
   /* 0x3d */ {INVALID},
   /* 0x3e */ {INVALID},
   /* 0x3f */ {INVALID},

   /* 0x40 */ {VALID,      "cmovo",   AM_G, OT_V, AM_E, OT_V},
   /* 0x41 */ {VALID,      "cmovno",  AM_G, OT_V, AM_E, OT_V},
   /* 0x42 */ {VALID,      "cmovb",   AM_G, OT_V, AM_E, OT_V},
   /* 0x43 */ {VALID,      "cmovnb",  AM_G, OT_V, AM_E, OT_V},
   /* 0x44 */ {VALID,      "cmove",   AM_G, OT_V, AM_E, OT_V},
   /* 0x45 */ {VALID,      "cmovne",  AM_G, OT_V, AM_E, OT_V},
   /* 0x46 */ {VALID,      "cmovna",  AM_G, OT_V, AM_E, OT_V},
   /* 0x47 */ {VALID,      "cmovnba", AM_G, OT_V, AM_E, OT_V},
   /* 0x48 */ {VALID,      "cmovs",   AM_G, OT_V, AM_E, OT_V},
   /* 0x49 */ {VALID,      "cmovns",  AM_G, OT_V, AM_E, OT_V},
   /* 0x4a */ {VALID,      "cmovp",   AM_G, OT_V, AM_E, OT_V},
   /* 0x4b */ {VALID,      "cmovnp",  AM_G, OT_V, AM_E, OT_V},
   /* 0x4c */ {VALID,      "cmovl",   AM_G, OT_V, AM_E, OT_V},
   /* 0x4d */ {VALID,      "cmovnl",  AM_G, OT_V, AM_E, OT_V},
   /* 0x4e */ {VALID,      "cmovle",  AM_G, OT_V, AM_E, OT_V},
   /* 0x4f */ {VALID,      "cmovnle", AM_G, OT_V, AM_E, OT_V},

   /* 0x50 */ {VALID,      "movmskps", AM_E, OT_V, AM_V, OT_V},
   /* 0x51 */ {VALID,      "sqrtps",   AM_V, OT_V, AM_E, OT_V},
   /* 0x52 */ {VALID,      "rsqrtps",  AM_V, OT_V, AM_E, OT_V},
   /* 0x53 */ {VALID,      "rcpps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x54 */ {VALID,      "andps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x55 */ {VALID,      "andnps",   AM_V, OT_V, AM_E, OT_V},
   /* 0x56 */ {VALID,      "orps",     AM_V, OT_V, AM_E, OT_V},
   /* 0x57 */ {VALID,      "xorps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x58 */ {VALID,      "addps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x59 */ {VALID,      "mulps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x5a */ {VALID,      "cvtps2pd", AM_V, OT_V, AM_E, OT_V},     // #EMP# Was: {INVALID},
   /* 0x5b */ {VALID,      "cvtdq2ps", AM_V, OT_V, AM_E, OT_V},     // #EMP# Was: {INVALID},
   /* 0x5c */ {VALID,      "subps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x5d */ {VALID,      "minps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x5e */ {VALID,      "divps",    AM_V, OT_V, AM_E, OT_V},
   /* 0x5f */ {VALID,      "maxps",    AM_V, OT_V, AM_E, OT_V},

   /* 0x60 */ {VALID,      "punpcklbw", AM_P, OT_Q, AM_E, OT_V},
   /* 0x61 */ {VALID,      "punpcklwd", AM_P, OT_Q, AM_E, OT_V},
   /* 0x62 */ {VALID,      "punpckldq", AM_P, OT_Q, AM_E, OT_V},
   /* 0x63 */ {VALID,      "packsswb",  AM_P, OT_Q, AM_E, OT_V},    // #EMP# Was: "punpckswb"
   /* 0x64 */ {VALID,      "pcmpgtb",   AM_P, OT_Q, AM_E, OT_V},    // #EMP# Was: "punpckgtb"
   /* 0x65 */ {VALID,      "pcmpgtw",   AM_P, OT_Q, AM_E, OT_V},    // #EMP# Was: "punpckgtw"
   /* 0x66 */ {VALID,      "pcmpgtd",   AM_P, OT_Q, AM_E, OT_V},    // #EMP# Was: "punpckgtd"
   /* 0x67 */ {VALID,      "packuswb",  AM_P, OT_Q, AM_E, OT_V},    // #EMP# Was: "punpckswb"
   /* 0x68 */ {VALID,      "punpckhbw", AM_P, OT_Q, AM_E, OT_V},
   /* 0x69 */ {VALID,      "punpckhwd", AM_P, OT_Q, AM_E, OT_V},
   /* 0x6a */ {VALID,      "punpckhdq", AM_P, OT_Q, AM_E, OT_V},
   /* 0x6b */ {VALID,      "packssdw",  AM_P, OT_Q, AM_E, OT_V},    // #EMP# Was: "punpcksdw"
   /* 0x6c */ {VALID,      "punpcklqdq", AM_P, OT_Q, AM_E, OT_V},   // #EMP# Was: {INVALID},
   /* 0x6d */ {VALID,      "punpckhqdq", AM_P, OT_Q, AM_E, OT_V},   // #EMP# Was: {INVALID},
   /* 0x6e */ {VALID,      "movd",      AM_P, OT_Q, AM_E, OT_V},
   /* 0x6f */ {VALID,      "movq",      AM_P, OT_Q, AM_E, OT_V},

   /* 0x70 */ {VALID,      "pshufw",    AM_P, OT_Q, AM_E, OT_Q},
   /* 0x71 */ {SUBTB, (char *)g12_t, 0, 0, 0, 0, 0, 0, OC_MASK, 3},
   /* 0x72 */ {SUBTB, (char *)g13_t, 0, 0, 0, 0, 0, 0, OC_MASK, 3},
   /* 0x73 */ {SUBTB, (char *)g14_t, 0, 0, 0, 0, 0, 0, OC_MASK, 4}, //#EMP#
   /* 0x74 */ {VALID,      "pcmpeqb",   AM_P, OT_Q, AM_E, OT_Q},
   /* 0x75 */ {VALID,      "pcmpeqw",   AM_P, OT_Q, AM_E, OT_Q},
   /* 0x76 */ {VALID,      "pcmpeqd",   AM_P, OT_Q, AM_E, OT_Q},
   /* 0x77 */ {VALID,      "emms"},
   /* 0x78 */ {VALID,      "vmread",    AM_E, OT_D, AM_G, OT_D},    // #EMP# Was: {VALID,      "mmxud"},
   /* 0x79 */ {VALID,      "vmwrite",   AM_G, OT_D, AM_E, OT_D},    // #EMP# Was: {VALID,      "mmxud"},
   /* 0x7a */ {INVALID},                                            // #EMP# Was: {VALID,      "mmxud"},
   /* 0x7b */ {INVALID},                                            // #EMP# Was: {VALID,      "mmxud"},
   /* 0x7c */ {VALID,      "haddps",    AM_V, OT_PS, AM_W, OT_PS},  // #EMP# Was: {VALID,      "mmxud"},
   /* 0x7d */ {VALID,      "hsubps",    AM_V, OT_PS, AM_W, OT_PS},  // #EMP# Was: {VALID,      "mmxud"},
   /* 0x7e */ {VALID,      "movd", AM_E, OT_D, AM_P, OT_D},
   /* 0x7f */ {VALID,      "movq", AM_E, OT_Q, AM_P, OT_Q},

   /* 0x80 */ {VALID,      "jo",     AM_J, OT_V},
   /* 0x81 */ {VALID,      "jno",    AM_J, OT_V},
   /* 0x82 */ {VALID,      "jb",     AM_J, OT_V},
   /* 0x83 */ {VALID,      "jnb",    AM_J, OT_V},
   /* 0x84 */ {VALID,      "jz",     AM_J, OT_V},
   /* 0x85 */ {VALID,      "jnz",    AM_J, OT_V},
   /* 0x86 */ {VALID,      "jbe",    AM_J, OT_V},
   /* 0x87 */ {VALID,      "jnbe",   AM_J, OT_V},
   /* 0x88 */ {VALID,      "js",     AM_J, OT_V},
   /* 0x89 */ {VALID,      "jns",    AM_J, OT_V},
   /* 0x8a */ {VALID,      "jp",     AM_J, OT_V},
   /* 0x8b */ {VALID,      "jnp",    AM_J, OT_V},
   /* 0x8c */ {VALID,      "jl",     AM_J, OT_V},
   /* 0x8d */ {VALID,      "jnl",    AM_J, OT_V},
   /* 0x8e */ {VALID,      "jle",    AM_J, OT_V},
   /* 0x8f */ {VALID,      "jnle",   AM_J, OT_V},

   /* 0x90 */ {VALID,      "seto",   AM_E, OT_B},
   /* 0x91 */ {VALID,      "setno",  AM_E, OT_B},
   /* 0x92 */ {VALID,      "setb",   AM_E, OT_B},
   /* 0x93 */ {VALID,      "setnb",  AM_E, OT_B},
   /* 0x94 */ {VALID,      "setz",   AM_E, OT_B},
   /* 0x95 */ {VALID,      "setnz",  AM_E, OT_B},
   /* 0x96 */ {VALID,      "setbe",  AM_E, OT_B},
   /* 0x97 */ {VALID,      "setnbe", AM_E, OT_B},
   /* 0x98 */ {VALID,      "sets",   AM_E, OT_B},
   /* 0x99 */ {VALID,      "setns",  AM_E, OT_B},
   /* 0x9a */ {VALID,      "setp",   AM_E, OT_B},
   /* 0x9b */ {VALID,      "setnp",  AM_E, OT_B},
   /* 0x9c */ {VALID,      "setl",   AM_E, OT_B},
   /* 0x9d */ {VALID,      "setnl",  AM_E, OT_B},
   /* 0x9e */ {VALID,      "setle",  AM_E, OT_B},
   /* 0x9f */ {VALID,      "setnle", AM_E, OT_B},

   /* 0xa0 */ {VALID,      "push",   FS},
   /* 0xa1 */ {VALID,      "pop",    FS},
   /* 0xa2 */ {VALID,      "cpuid"},
   /* 0xa3 */ {VALID,      "bt",     AM_E, OT_V, AM_G, OT_V},
   /* 0xa4 */ {VALID,      "shld",   AM_E, OT_V, AM_G, OT_V, AM_I, OT_B},
   /* 0xa5 */ {VALID,      "shld",   AM_E, OT_V, AM_G, OT_V, CL},
   /* 0xa6 */ {INVALID},                                            // #EMP# Was: {VALID,      "xbts",   AM_E, OT_V, AM_G, OT_V},
   /* 0xa7 */ {INVALID},
   /* 0xa8 */ {VALID,      "push",   GS},
   /* 0xa9 */ {VALID,      "pop",    GS},
   /* 0xaa */ {VALID,      "rsm"},
   /* 0xab */ {VALID,      "bts",    AM_E, OT_V, AM_G, OT_V},
   /* 0xac */ {VALID,      "shrd",   AM_E, OT_V, AM_G, OT_V, AM_I, OT_B},
   /* 0xad */ {VALID,      "shrd",   AM_E, OT_V, AM_G, OT_V,CL},
   /* 0xae */ {SUBTB,(char *)g15_t,  AM_M, OT_P, AM_M, OT_P, 0, 0, OC_MASK, 7},   //STJ
   /* 0xaf */ {VALID,      "imul",   AM_G, OT_V, AM_E, OT_V},

   /* 0xb0 */ {VALID,      "cmpxch", AM_E, OT_B, AM_G, OT_B},
   /* 0xb1 */ {VALID,      "cmpxch", AM_E, OT_V, AM_G, OT_V},
   /* 0xb2 */ {VALID,      "lss",    AM_M, OT_P},
   /* 0xb3 */ {VALID,      "btr",    AM_E, OT_V, AM_G, OT_V},
   /* 0xb4 */ {VALID,      "lfs",    AM_M, OT_P},
   /* 0xb5 */ {VALID,      "lgs",    AM_M, OT_P},
   /* 0xb6 */ {VALID,      "movzx",  AM_G, OT_V, AM_E, OT_B},
   /* 0xb7 */ {VALID,      "movzx",  AM_G, OT_V, AM_E, OT_W},
   /* 0xb8 */ {INVALID},                                            // #EMP# JMPE (reserved for emulator on IPF)
   /* 0xb9 */ {VALID,      "inv_0fb9"},
   /* 0xba */ {SUBTB,(char *)g8_t, AM_E, OT_V, AM_I, OT_B, 0, 0, OC_MASK, 4},
   /* 0xbb */ {VALID,      "btc",    AM_E, OT_V, AM_G, OT_V},
   /* 0xbc */ {VALID,      "bsf",    AM_G, OT_V, AM_E, OT_V},
   /* 0xbd */ {VALID,      "bsr",    AM_G, OT_V, AM_E, OT_V},
   /* 0xbe */ {VALID,      "movsx",  AM_G, OT_V, AM_E, OT_B},
   /* 0xbf */ {VALID,      "movsx",  AM_G, OT_V, AM_E, OT_W},

   /* 0xc0 */ {VALID,      "xadd",    AM_E, OT_B, AM_G, OT_B},
   /* 0xc1 */ {VALID,      "xadd",    AM_E, OT_V, AM_G, OT_V},
   /* 0xc2 */ {VALID,      "cmpps",   AM_V, OT_V, AM_E, OT_V},
   /* 0xc3 */ {VALID,      "movnti",  AM_G, OT_V, AM_E, OT_V},   //STJ
   /* 0xc4 */ {VALID,      "pinsrw",  AM_P, OT_V, AM_E, OT_V},
   /* 0xc5 */ {VALID,      "pextrw",  AM_G, OT_V, AM_P, OT_V},
   /* 0xc6 */ {VALID,      "shufps",  AM_V, OT_V, AM_E, OT_V},
   /* 0xc7 */ {SUBTB,(char *)g9_t,   0,    0,    0,    0, 0, 0, OC_MASK, 3},
   /* 0xc8 */ {VALID,      "bswap",  EAX},                          // #EMP# bswap RAX/EAX/R8/R8D
   /* 0xc9 */ {VALID,      "bswap",  ECX},                          // #EMP# bswap RCX/ECX/R9/R9D
   /* 0xca */ {VALID,      "bswap",  EDX},                          // #EMP# bswap RDX/ECX/R10/R10D
   /* 0xcb */ {VALID,      "bswap",  EBX},                          // #EMP# bswap RBX/EBX/R11/R11D
   /* 0xcc */ {VALID,      "bswap",  ESP},                          // #EMP# bswap RSP/ESP/R12/R12D
   /* 0xcd */ {VALID,      "bswap",  EBP},                          // #EMP# bswap RBP/EBP/R13/R13D
   /* 0xce */ {VALID,      "bswap",  ESI},                          // #EMP# bswap RSI/ESI/R14/R14D
   /* 0xcf */ {VALID,      "bswap",  EDI},                          // #EMP# bswap RDI/EDI/R15/R15D

   /* 0xd0 */ {VALID,    "addsubpd",  AM_P, OT_Q, AM_E, OT_V},   //STJ
   /* 0xd1 */ {VALID,      "psrlw",   AM_P, OT_V, AM_E, OT_V},
   /* 0xd2 */ {VALID,      "psrld",   AM_P, OT_V, AM_E, OT_V},
   /* 0xd3 */ {VALID,      "psrlq",   AM_P, OT_V, AM_E, OT_V},
   /* 0xd4 */ {VALID,      "paddq",   AM_P, OT_V, AM_E, OT_V},
   /* 0xd5 */ {VALID,      "pmullw",  AM_P, OT_V, AM_E, OT_V},
   /* 0xd6 */ {VALID,      "movq",    AM_E, OT_Q, AM_P, OT_Q},   //STJ
   /* 0xd7 */ {VALID,     "pmovmskb", AM_P, OT_V, AM_E, OT_V},
   /* 0xd8 */ {VALID,      "psubusb", AM_P, OT_V, AM_E, OT_V},
   /* 0xd9 */ {VALID,      "psubusw", AM_P, OT_V, AM_E, OT_V},
   /* 0xda */ {VALID,      "pminub",  AM_P, OT_V, AM_E, OT_V},
   /* 0xdb */ {VALID,      "pand",    AM_P, OT_V, AM_E, OT_V},
   /* 0xdc */ {VALID,      "paddusb", AM_P, OT_V, AM_E, OT_V},
   /* 0xdd */ {VALID,      "paddusw", AM_P, OT_V, AM_E, OT_V},
   /* 0xde */ {VALID,      "pmaxub,", AM_P, OT_V, AM_E, OT_V},
   /* 0xdf */ {VALID,      "pandh",   AM_P, OT_V, AM_E, OT_V},

   /* 0xe0 */ {VALID,      "pavgb",   AM_P, OT_Q, AM_E, OT_V},
   /* 0xe1 */ {VALID,      "psraw",   AM_P, OT_Q, AM_E, OT_V},
   /* 0xe2 */ {VALID,      "psrad",   AM_P, OT_Q, AM_E, OT_V},
   /* 0xe3 */ {VALID,      "pavgw",   AM_P, OT_Q, AM_E, OT_V},
   /* 0xe4 */ {VALID,      "pmulhuw", AM_P, OT_Q, AM_E, OT_V},
   /* 0xe5 */ {VALID,      "pmulhw",  AM_P, OT_Q, AM_E, OT_V},
   /* 0xe6 */ {VALID,     "cvtpd2dq", AM_P, OT_Q, AM_E, OT_Q},      // #EMP# Was: "cvtdq"
   /* 0xe7 */ {VALID,      "movntq",  AM_E, OT_Q, AM_V, OT_V},
   /* 0xe8 */ {VALID,      "psubsb",  AM_P, OT_Q, AM_E, OT_V},
   /* 0xe9 */ {VALID,      "psubsw",  AM_P, OT_Q, AM_E, OT_V},      // #EMP# Was: "psubsb"
   /* 0xea */ {VALID,      "pminsw",  AM_P, OT_Q, AM_E, OT_V},
   /* 0xeb */ {VALID,      "por",     AM_P, OT_Q, AM_E, OT_V},
   /* 0xec */ {VALID,      "paddsb",  AM_P, OT_Q, AM_E, OT_V},
   /* 0xed */ {VALID,      "paddsw",  AM_P, OT_Q, AM_E, OT_V},
   /* 0xee */ {VALID,      "pmaxsw",  AM_P, OT_Q, AM_E, OT_V},
   /* 0xef */ {VALID,      "pxor",    AM_P, OT_Q, AM_E, OT_V},

   /* 0xf0 */ {VALID,      "lddqu",    AM_P, OT_Q, AM_E, OT_V},     // #EMP# Was: {INVALID},
   /* 0xf1 */ {VALID,      "psllw",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xf2 */ {VALID,      "pslld",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xf3 */ {VALID,      "psllq",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xf4 */ {VALID,      "pmuludq",  AM_P, OT_Q, AM_E, OT_V},     // #EMP# Was: {INVALID},
   /* 0xf5 */ {VALID,      "pmaddwd",  AM_P, OT_Q, AM_E, OT_V},
   /* 0xf6 */ {VALID,      "psadbw",   AM_P, OT_Q, AM_E, OT_V},
   /* 0xf7 */ {VALID,      "maskmovq", AM_P, OT_Q, AM_E, OT_V},
   /* 0xf8 */ {VALID,      "psubb",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xf9 */ {VALID,      "psubw",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xfa */ {VALID,      "psubd",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xfb */ {VALID,      "psubq",    AM_P, OT_Q, AM_E, OT_V},     // #EMP# Was: {INVALID},
   /* 0xfc */ {VALID,      "paddb",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xfd */ {VALID,      "paddw",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xfe */ {VALID,      "paddd",    AM_P, OT_Q, AM_E, OT_V},
   /* 0xff */ {INVALID}
};



// ** Floating point instructions with 0xD8 escape code
static INSTRUCTION d8a_t[] =
{
   {0x00 | VALID,    "fadd"},
   {0x08 | VALID,    "fmul"},
   {0x10 | VALID,    "fcom"},
   {0x18 | VALID,   "fcomp"},
   {0x20 | VALID,    "fsub"},
   {0x28 | VALID,   "fsubr"},
   {0x30 | VALID,    "fdiv"},
   {0x38 | VALID,   "fdivr"}
};

static INSTRUCTION d8b_t[] =
{
   {0x00 | VALID,    "fadd"},
   {0x08 | VALID,    "fmul"},
   {0x10 | VALID,    "fcom"},
   {0x18 | VALID,   "fcomp"},
   {0x20 | VALID,    "fsub"},
   {0x28 | VALID,   "fsubr"},
   {0x30 | VALID,    "fdiv"},
   {0x38 | VALID,   "fdivr"}
};

static INSTRUCTION d8_t[] =
{
   {0x00 | SUBTB, (char *)d8a_t, AM_E,  OT_SI, 0, 0, 0, 0, OC_MASK, 8},
   {0x40 | SUBTB, (char *)d8a_t, AM_E,  OT_SI, 0, 0, 0, 0, OC_MASK, 8},
   {0x80 | SUBTB, (char *)d8a_t, AM_E,  OT_SI, 0, 0, 0, 0, OC_MASK, 8},
   {0xc0 | SUBTB, (char *)d8b_t, ST, 0, ST_NUM, 0, 0, 0, OC_MASK, 8}
};


// ** Floating point instructions with 0xD9 escape code
static INSTRUCTION d9a_t[] =
{
   {0x00 | VALID,   "fld",    AM_E, OT_D},
   {0x10 | VALID,   "fst",    AM_E, OT_D},
   {0x18 | VALID,   "fstp",   AM_E, OT_D},
   {0x20 | VALID,   "fldenv", AM_E, OT_V},
   {0x28 | VALID,   "fldcw",  AM_E, OT_W},
   {0x30 | VALID,   "fstenv", AM_E, OT_V},
   {0x38 | VALID,   "fstcw",  AM_E, OT_W}
};

static INSTRUCTION d9b_t[] =
{
   {0xc0 | VALID,   "fld",    ST, 0, ST_NUM},
   {0xc1 | VALID,   "fld",    ST, 0, ST_NUM},
   {0xc2 | VALID,   "fld",    ST, 0, ST_NUM},
   {0xc3 | VALID,   "fld",    ST, 0, ST_NUM},
   {0xc4 | VALID,   "fld",    ST, 0, ST_NUM},
   {0xc5 | VALID,   "fld",    ST, 0, ST_NUM},
   {0xc6 | VALID,   "fld",    ST, 0, ST_NUM},
   {0xc7 | VALID,   "fld",    ST, 0, ST_NUM},
   {0xc8 | VALID,   "fxch",   ST, 0, ST_NUM},
   {0xc9 | VALID,   "fxch",   ST, 0, ST_NUM},
   {0xca | VALID,   "fxch",   ST, 0, ST_NUM},
   {0xcb | VALID,   "fxch",   ST, 0, ST_NUM},
   {0xcc | VALID,   "fxch",   ST, 0, ST_NUM},
   {0xcd | VALID,   "fxch",   ST, 0, ST_NUM},
   {0xce | VALID,   "fxch",   ST, 0, ST_NUM},
   {0xcf | VALID,   "fxch",   ST, 0, ST_NUM},
   {0xd0 | VALID,   "fnop"},
   {0xe0 | VALID,   "fchs"},
   {0xe1 | VALID,   "fabs"},
   {0xe4 | VALID,   "ftst"},
   {0xe5 | VALID,   "fxam"},
   {0xe8 | VALID,   "fld1"},
   {0xe9 | VALID,   "fldl2t"},
   {0xea | VALID,   "fldl2e"},
   {0xeb | VALID,   "fldpi"},
   {0xec | VALID,   "fldlg2"},
   {0xed | VALID,   "fldln2"},
   {0xee | VALID,   "fldz"},
   {0xf0 | VALID,   "f2xm1"},
   {0xf1 | VALID,   "fyl2x"},
   {0xf2 | VALID,   "fptan"},
   {0xf3 | VALID,   "fpatan"},
   {0xf4 | VALID,   "fxtract"},
   {0xf5 | VALID,   "fprem1"},
   {0xf6 | VALID,   "fdecstp"},
   {0xf7 | VALID,   "fincstp"},
   {0xf8 | VALID,   "fprem"},
   {0xf9 | VALID,   "fyl2xp1"},
   {0xfa | VALID,   "fsqrt"},
   {0xfb | VALID,   "fsincos"},
   {0xfc | VALID,   "frndint"},
   {0xfd | VALID,   "fscale"},
   {0xfe | VALID,   "fsin"},
   {0xff | VALID,   "fcos"}
};

static INSTRUCTION d9_t[] =
{
   {0x00 | SUBTB, (char *)d9a_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK,  7},
   {0x40 | SUBTB, (char *)d9a_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK,  7},
   {0x80 | SUBTB, (char *)d9a_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK,  7},
   {0xc0 | SUBTB, (char *)d9b_t, 0, 0, 0, 0, 0, 0, BYTE_MASK, 44}
};


// ** Floating point instructions with 0xDA escape code
static INSTRUCTION daa_t[] =
{
   {0x00 | VALID,    "fiadd"},
   {0x08 | VALID,    "fimul"},
   {0x10 | VALID,    "ficom"},
   {0x18 | VALID,   "ficomp"},
   {0x20 | VALID,    "fisub"},
   {0x28 | VALID,   "fisubr"},
   {0x30 | VALID,    "fidiv"},
   {0x38 | VALID,   "fidivr"}
};

static INSTRUCTION dab_t[] =
{
   {0xc0 | VALID,   "fcmovb", ST, 0, ST_NUM},
   {0xc1 | VALID,   "fcmovb", ST, 0, ST_NUM},
   {0xc2 | VALID,   "fcmovb", ST, 0, ST_NUM},
   {0xc3 | VALID,   "fcmovb", ST, 0, ST_NUM},
   {0xc4 | VALID,   "fcmovb", ST, 0, ST_NUM},
   {0xc5 | VALID,   "fcmovb", ST, 0, ST_NUM},
   {0xc6 | VALID,   "fcmovb", ST, 0, ST_NUM},
   {0xc7 | VALID,   "fcmovb", ST, 0, ST_NUM},

   {0xc8 | VALID,   "fcmove", ST, 0, ST_NUM},
   {0xc9 | VALID,   "fcmove", ST, 0, ST_NUM},
   {0xca | VALID,   "fcmove", ST, 0, ST_NUM},
   {0xcb | VALID,   "fcmove", ST, 0, ST_NUM},
   {0xcc | VALID,   "fcmove", ST, 0, ST_NUM},
   {0xcd | VALID,   "fcmove", ST, 0, ST_NUM},
   {0xce | VALID,   "fcmove", ST, 0, ST_NUM},
   {0xcf | VALID,   "fcmove", ST, 0, ST_NUM},

   {0xd0 | VALID,   "fcmovbe", ST, 0, ST_NUM},
   {0xd1 | VALID,   "fcmovbe", ST, 0, ST_NUM},
   {0xd2 | VALID,   "fcmovbe", ST, 0, ST_NUM},
   {0xd3 | VALID,   "fcmovbe", ST, 0, ST_NUM},
   {0xd4 | VALID,   "fcmovbe", ST, 0, ST_NUM},
   {0xd5 | VALID,   "fcmovbe", ST, 0, ST_NUM},
   {0xd6 | VALID,   "fcmovbe", ST, 0, ST_NUM},
   {0xd7 | VALID,   "fcmovbe", ST, 0, ST_NUM},

   {0xd8 | VALID,   "fcmovu", ST, 0, ST_NUM},
   {0xd9 | VALID,   "fcmovu", ST, 0, ST_NUM},
   {0xda | VALID,   "fcmovu", ST, 0, ST_NUM},
   {0xdb | VALID,   "fcmovu", ST, 0, ST_NUM},
   {0xdc | VALID,   "fcmovu", ST, 0, ST_NUM},
   {0xdd | VALID,   "fcmovu", ST, 0, ST_NUM},
   {0xde | VALID,   "fcmovu", ST, 0, ST_NUM},
   {0xdf | VALID,   "fcmovu", ST, 0, ST_NUM},

   {0xe9 | VALID, "fucompp"}
};

static INSTRUCTION da_t[] =
{
   {0x00 | SUBTB, (char *)daa_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK, 8},
   {0x40 | SUBTB, (char *)daa_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK, 8},
   {0x80 | SUBTB, (char *)daa_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK, 8},
   {0xc0 | SUBTB, (char *)dab_t,    0,     0, 0, 0, 0, 0, BYTE_MASK, 33}
};


// ** Floating point instructions with 0xDB escape code
static INSTRUCTION dba_t[] =
{
   {0x00 | VALID,    "fild", AM_E, OT_SI},
   {0x08 | VALID,  "fisttp", AM_E, OT_SI},
   {0x10 | VALID,    "fist", AM_E, OT_SI},  // #EMP#
   {0x18 | VALID,   "fistp", AM_E, OT_SI},
   {0x28 | VALID,     "fld", AM_E, OT_Q},   /* 10 bytes not 8 ?? */
   {0x38 | VALID,    "fstp", AM_E, OT_Q}    /* 10 bytes not 8 ?? */
};                                          /* disasm error, not leng error */

static INSTRUCTION dbb_t[] =
{
   {0xc0 | VALID,   "fcmovnb", ST, 0, ST_NUM},
   {0xc1 | VALID,   "fcmovnb", ST, 0, ST_NUM},
   {0xc2 | VALID,   "fcmovnb", ST, 0, ST_NUM},
   {0xc3 | VALID,   "fcmovnb", ST, 0, ST_NUM},
   {0xc4 | VALID,   "fcmovnb", ST, 0, ST_NUM},
   {0xc5 | VALID,   "fcmovnb", ST, 0, ST_NUM},
   {0xc6 | VALID,   "fcmovnb", ST, 0, ST_NUM},
   {0xc7 | VALID,   "fcmovnb", ST, 0, ST_NUM},

   {0xc8 | VALID,   "fcmovne", ST, 0, ST_NUM},
   {0xc9 | VALID,   "fcmovne", ST, 0, ST_NUM},
   {0xca | VALID,   "fcmovne", ST, 0, ST_NUM},
   {0xcb | VALID,   "fcmovne", ST, 0, ST_NUM},
   {0xcc | VALID,   "fcmovne", ST, 0, ST_NUM},
   {0xcd | VALID,   "fcmovne", ST, 0, ST_NUM},
   {0xce | VALID,   "fcmovne", ST, 0, ST_NUM},
   {0xcf | VALID,   "fcmovne", ST, 0, ST_NUM},

   {0xd0 | VALID,   "fcmovnbe", ST, 0, ST_NUM},
   {0xd1 | VALID,   "fcmovnbe", ST, 0, ST_NUM},
   {0xd2 | VALID,   "fcmovnbe", ST, 0, ST_NUM},
   {0xd3 | VALID,   "fcmovnbe", ST, 0, ST_NUM},
   {0xd4 | VALID,   "fcmovnbe", ST, 0, ST_NUM},
   {0xd5 | VALID,   "fcmovnbe", ST, 0, ST_NUM},
   {0xd6 | VALID,   "fcmovnbe", ST, 0, ST_NUM},
   {0xd7 | VALID,   "fcmovnbe", ST, 0, ST_NUM},

   {0xd8 | VALID,   "fcmovnu", ST, 0, ST_NUM},
   {0xd9 | VALID,   "fcmovnu", ST, 0, ST_NUM},
   {0xda | VALID,   "fcmovnu", ST, 0, ST_NUM},
   {0xdb | VALID,   "fcmovnu", ST, 0, ST_NUM},
   {0xdc | VALID,   "fcmovnu", ST, 0, ST_NUM},
   {0xdd | VALID,   "fcmovnu", ST, 0, ST_NUM},
   {0xde | VALID,   "fcmovnu", ST, 0, ST_NUM},
   {0xdf | VALID,   "fcmovnu", ST, 0, ST_NUM},

   {0xe2 | VALID,   "fclex"},
   {0xe3 | VALID,   "finit"},

   {0xe8 | VALID,   "fucomi", ST, 0, ST_NUM},
   {0xe9 | VALID,   "fucomi", ST, 0, ST_NUM},
   {0xea | VALID,   "fucomi", ST, 0, ST_NUM},
   {0xeb | VALID,   "fucomi", ST, 0, ST_NUM},
   {0xec | VALID,   "fucomi", ST, 0, ST_NUM},
   {0xed | VALID,   "fucomi", ST, 0, ST_NUM},
   {0xee | VALID,   "fucomi", ST, 0, ST_NUM},
   {0xef | VALID,   "fucomi", ST, 0, ST_NUM},

   {0xf0 | VALID,   "fcomi", ST, 0, ST_NUM},
   {0xf1 | VALID,   "fcomi", ST, 0, ST_NUM},
   {0xf2 | VALID,   "fcomi", ST, 0, ST_NUM},
   {0xf3 | VALID,   "fcomi", ST, 0, ST_NUM},
   {0xf4 | VALID,   "fcomi", ST, 0, ST_NUM},
   {0xf5 | VALID,   "fcomi", ST, 0, ST_NUM},
   {0xf6 | VALID,   "fcomi", ST, 0, ST_NUM},
   {0xf7 | VALID,   "fcomi", ST, 0, ST_NUM}
};

static INSTRUCTION db_t[] =
{
   {0x00 | SUBTB, (char *)dba_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK, 6},
   {0x40 | SUBTB, (char *)dba_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK, 6},
   {0x80 | SUBTB, (char *)dba_t, AM_E, OT_SI, 0, 0, 0, 0,   OC_MASK, 6},
   {0xc0 | SUBTB, (char *)dbb_t, 0, 0, 0, 0, 0, 0, BYTE_MASK, 50}
};


// ** Floating point instructions with 0xDC escape code
static INSTRUCTION dca_t[] =
{
   {0x00 | VALID,   "fadd"},
   {0x08 | VALID,   "fmul"},
   {0x10 | VALID,   "fcom"},
   {0x18 | VALID,  "fcomp"},
   {0x20 | VALID,   "fsub"},
   {0x28 | VALID,  "fsubr"},
   {0x30 | VALID,   "fdiv"},
   {0x38 | VALID,  "fdivr"}
};

static INSTRUCTION dcb_t[] =
{
   {0x00 | VALID,   "fadd"},
   {0x08 | VALID,   "fmul"},
   {0x20 | VALID,  "fsubr"},
   {0x28 | VALID,   "fsub"},
   {0x30 | VALID,  "fdivr"},
   {0x38 | VALID,   "fdiv"}
};

static INSTRUCTION dc_t[] =
{
   {0x00 | SUBTB, (char *)dca_t, AM_E, OT_Q, 0, 0, 0, 0, OC_MASK, 8},
   {0x40 | SUBTB, (char *)dca_t, AM_E, OT_Q, 0, 0, 0, 0, OC_MASK, 8},
   {0x80 | SUBTB, (char *)dca_t, AM_E, OT_Q, 0, 0, 0, 0, OC_MASK, 8},
   {0xc0 | SUBTB, (char *)dcb_t, ST_NUM, 0, ST, 0, 0, 0, OC_MASK, 6}
};


/* OT_W below ??? */
// ** Floating point instructions with 0xDD escape code
static INSTRUCTION dda_t[] =
{
   {0x00 | VALID,   "fld",  AM_E, OT_W},
   {0x08 | VALID, "fistp",  AM_E, OT_W},   // #EMP#
   {0x10 | VALID,   "fst",  AM_E, OT_W},
   {0x18 | VALID,  "fstp",  AM_E, OT_W},
   {0x20 | VALID, "frstor", AM_E, OT_W},
   {0x30 | VALID,  "fsave", AM_E, OT_W},
   {0x38 | VALID,  "fstsw", AM_E, OT_W}
};

static INSTRUCTION ddb_t[] =
{
   {0x00 | VALID,  "ffree", ST_NUM},
   {0x10 | VALID,    "fst", ST_NUM},
   {0x18 | VALID,   "fstp", ST_NUM},
   {0x20 | VALID,  "fucom", ST_NUM, 0, ST},
   {0x28 | VALID, "fucomp", ST_NUM}
};

static INSTRUCTION dd_t[] =
{
   {0x00 | SUBTB,  (char *)dda_t, AM_E, OT_Q, 0, 0, 0, 0, OC_MASK, 7},
   {0x40 | SUBTB,  (char *)dda_t, AM_E, OT_Q, 0, 0, 0, 0, OC_MASK, 7},
   {0x80 | SUBTB,  (char *)dda_t, AM_E, OT_Q, 0, 0, 0, 0, OC_MASK, 7},
   {0xc0 | SUBTB,  (char *)ddb_t, 0, 0, 0, 0, 0, 0, OC_MASK, 5}
};


// ** Floating point instructions with 0xDE escape code
static INSTRUCTION dea_t[] =
{
   {0x00 | VALID,   "fiadd"},
   {0x08 | VALID,   "fimul"},
   {0x10 | VALID,   "ficom"},
   {0x18 | VALID,  "ficomp"},
   {0x20 | VALID,   "fisub"},
   {0x28 | VALID,  "fisubr"},
   {0x30 | VALID,   "fidiv"},
   {0x38 | VALID,  "fidivr"}
};

/* ??? rju 070397 */
/* wrong operand decode for ded9 */
/* appear (r) is taken from de vs d9 ??? */
/* correct : fcompp  st(1), st  */
/*    mine : fcompp  st(6), st  */

static INSTRUCTION deb_t[] =
{
   {0xc0 | VALID,   "faddp", ST_NUM, 0, ST},
   {0xc1 | VALID,   "faddp", ST_NUM, 0, ST},
   {0xc2 | VALID,   "faddp", ST_NUM, 0, ST},
   {0xc3 | VALID,   "faddp", ST_NUM, 0, ST},
   {0xc4 | VALID,   "faddp", ST_NUM, 0, ST},
   {0xc5 | VALID,   "faddp", ST_NUM, 0, ST},
   {0xc6 | VALID,   "faddp", ST_NUM, 0, ST},
   {0xc7 | VALID,   "faddp", ST_NUM, 0, ST},

   {0xc8 | VALID,   "fmulp", ST_NUM, 0, ST},
   {0xc9 | VALID,   "fmulp", ST_NUM, 0, ST},
   {0xca | VALID,   "fmulp", ST_NUM, 0, ST},
   {0xcb | VALID,   "fmulp", ST_NUM, 0, ST},
   {0xcc | VALID,   "fmulp", ST_NUM, 0, ST},
   {0xcd | VALID,   "fmulp", ST_NUM, 0, ST},
   {0xce | VALID,   "fmulp", ST_NUM, 0, ST},
   {0xcf | VALID,   "fmulp", ST_NUM, 0, ST},

   {0xd9 | VALID,   "fcompp"},

   {0xe0 | VALID,  "fsubrp", ST_NUM, 0, ST},
   {0xe1 | VALID,  "fsubrp", ST_NUM, 0, ST},
   {0xe2 | VALID,  "fsubrp", ST_NUM, 0, ST},
   {0xe3 | VALID,  "fsubrp", ST_NUM, 0, ST},
   {0xe4 | VALID,  "fsubrp", ST_NUM, 0, ST},
   {0xe5 | VALID,  "fsubrp", ST_NUM, 0, ST},
   {0xe6 | VALID,  "fsubrp", ST_NUM, 0, ST},
   {0xe7 | VALID,  "fsubrp", ST_NUM, 0, ST},

   {0xe8 | VALID,   "fsubp", ST_NUM, 0, ST},
   {0xe9 | VALID,   "fsubp", ST_NUM, 0, ST},
   {0xea | VALID,   "fsubp", ST_NUM, 0, ST},
   {0xeb | VALID,   "fsubp", ST_NUM, 0, ST},
   {0xec | VALID,   "fsubp", ST_NUM, 0, ST},
   {0xed | VALID,   "fsubp", ST_NUM, 0, ST},
   {0xee | VALID,   "fsubp", ST_NUM, 0, ST},
   {0xef | VALID,   "fsubp", ST_NUM, 0, ST},

   {0xf0 | VALID,  "fdivrp", ST_NUM, 0, ST},
   {0xf1 | VALID,  "fdivrp", ST_NUM, 0, ST},
   {0xf2 | VALID,  "fdivrp", ST_NUM, 0, ST},
   {0xf3 | VALID,  "fdivrp", ST_NUM, 0, ST},
   {0xf4 | VALID,  "fdivrp", ST_NUM, 0, ST},
   {0xf5 | VALID,  "fdivrp", ST_NUM, 0, ST},
   {0xf6 | VALID,  "fdivrp", ST_NUM, 0, ST},
   {0xf7 | VALID,  "fdivrp", ST_NUM, 0, ST},

   {0xf8 | VALID,   "fdivp", ST_NUM, 0, ST},
   {0xf9 | VALID,   "fdivp", ST_NUM, 0, ST},
   {0xfa | VALID,   "fdivp", ST_NUM, 0, ST},
   {0xfb | VALID,   "fdivp", ST_NUM, 0, ST},
   {0xfc | VALID,   "fdivp", ST_NUM, 0, ST},
   {0xfd | VALID,   "fdivp", ST_NUM, 0, ST},
   {0xfe | VALID,   "fdivp", ST_NUM, 0, ST},
   {0xff | VALID,   "fdivp", ST_NUM, 0, ST}
};

static INSTRUCTION de_t[] =
{
   {0x00 | SUBTB, (char *)dea_t, AM_E, OT_W,  0, 0, 0, 0, OC_MASK, 8},
   {0x40 | SUBTB, (char *)dea_t, AM_E, OT_W,  0, 0, 0, 0, OC_MASK, 8},
   {0x80 | SUBTB, (char *)dea_t, AM_E, OT_W,  0, 0, 0, 0, OC_MASK, 8},
   {0xc0 | SUBTB, (char *)deb_t, 0,       0,  0, 0, 0, 0, BYTE_MASK, 49}
};


// **  Floating point instructions with 0xDF escape code
static INSTRUCTION dfa_t[] =
{
   {0x00 | VALID,   "fild", AM_E, OT_W},
   {0x08 | VALID, "fisttp", AM_E, OT_W},
   {0x10 | VALID,   "fist", AM_E, OT_W},
   {0x18 | VALID,  "fistp", AM_E, OT_W},
   {0x20 | VALID,   "fbld", AM_E, OT_W},
   {0x28 | VALID,   "fild", AM_E, OT_LI},
   {0x30 | VALID,  "fbstp", AM_E, OT_W},
   {0x38 | VALID,  "fistp", AM_E, OT_LI}
};

static INSTRUCTION dfb_t[] =
{
   {0xe0 | VALID,  "fstsw", AX},

   {0xe8 | VALID,  "fucomip", ST, 0, ST_NUM},   // #EMP#
   {0xe9 | VALID,  "fucomip", ST, 0, ST_NUM},   // #EMP#
   {0xea | VALID,  "fucomip", ST, 0, ST_NUM},   // #EMP#
   {0xeb | VALID,  "fucomip", ST, 0, ST_NUM},   // #EMP#
   {0xec | VALID,  "fucomip", ST, 0, ST_NUM},   // #EMP#
   {0xed | VALID,  "fucomip", ST, 0, ST_NUM},   // #EMP#
   {0xee | VALID,  "fucomip", ST, 0, ST_NUM},   // #EMP#
   {0xef | VALID,  "fucomip", ST, 0, ST_NUM},   // #EMP#

   {0xf0 | VALID,   "fcomip", ST, 0, ST_NUM},   // #EMP#
   {0xf1 | VALID,   "fcomip", ST, 0, ST_NUM},   // #EMP#
   {0xf2 | VALID,   "fcomip", ST, 0, ST_NUM},   // #EMP#
   {0xf3 | VALID,   "fcomip", ST, 0, ST_NUM},   // #EMP#
   {0xf4 | VALID,   "fcomip", ST, 0, ST_NUM},   // #EMP#
   {0xf5 | VALID,   "fcomip", ST, 0, ST_NUM},   // #EMP#
   {0xf6 | VALID,   "fcomip", ST, 0, ST_NUM},   // #EMP#
   {0xf7 | VALID,   "fcomip", ST, 0, ST_NUM}    // #EMP#
};

static INSTRUCTION df_t[] =
{
   {0x00 | SUBTB, (char *)dfa_t, AM_E, OT_Q, 0, 0, 0, 0,   OC_MASK, 8},
   {0x40 | SUBTB, (char *)dfa_t, AM_E, OT_Q, 0, 0, 0, 0,   OC_MASK, 8},
   {0x80 | SUBTB, (char *)dfa_t, AM_E, OT_Q, 0, 0, 0, 0,   OC_MASK, 8},
   {0xc0 | SUBTB, (char *)dfb_t, 0, 0, 0, 0, 0, 0, BYTE_MASK, 17}
};


//----------------------------------------------------------------------
// Definitions of all one-byte instructions
// See Table A-2 in Appendix 2 of the "Intel 64 and IA-32 Architectures
// Software Developer's Manual", Volume 2B: Instruction Set Reference, N-Z
//----------------------------------------------------------------------
static INSTRUCTION i386_instruction_tbl[] =
{
   /* 00 */ {VALID,    "add",  AM_E, OT_B, AM_G, OT_B},
   /* 01 */ {VALID,    "add",  AM_E, OT_V, AM_G, OT_V},
   /* 02 */ {VALID,    "add",  AM_G, OT_B, AM_E, OT_B},
   /* 03 */ {VALID,    "add",  AM_G, OT_V, AM_E, OT_V},
   /* 04 */ {VALID,    "add",    AL,    0, AM_I, OT_B},
   /* 05 */ {VALID,    "add",   eAX,    0, AM_I, OT_V},
   /* 06 */ {VALID,   "push",    ES},
   /* 07 */ {VALID,    "pop",    ES},
   /* 08 */ {VALID,     "or",   AM_E, OT_B, AM_G, OT_B},
   /* 09 */ {VALID,     "or",   AM_E, OT_V, AM_G, OT_V},
   /* 0a */ {VALID,     "or",   AM_G, OT_B, AM_E, OT_B},
   /* 0b */ {VALID,     "or",   AM_G, OT_V, AM_E, OT_V},
   /* 0c */ {VALID,     "or",     AL,    0, AM_I, OT_B},
   /* 0d */ {VALID,     "or",    eAX,    0, AM_I, OT_V},
   /* 0e */ {VALID,   "push",   CS},
   /* 0f */ {INVALID},                  // 2-byte opcode escape. Never used, handled by unique code //STJ
   // was   {SUBTB, (char *)i386_instruction_tbl_2b, 0, 0, 0, 0, 0, 0, BYTE_MASK, 256},

   /* 10 */ {VALID,    "adc",   AM_E, OT_B, AM_G, OT_B},
   /* 11 */ {VALID,    "adc",   AM_E, OT_V, AM_G, OT_V},
   /* 12 */ {VALID,    "adc",   AM_G, OT_B, AM_E, OT_B},
   /* 13 */ {VALID,    "adc",   AM_G, OT_V, AM_E, OT_V},
   /* 14 */ {VALID,    "adc",     AL,    0, AM_I, OT_B},
   /* 15 */ {VALID,    "adc",    eAX,    0, AM_I, OT_V},
   /* 16 */ {VALID,    "push",   SS},
   /* 17 */ {VALID,    "pop",    SS},
   /* 18 */ {VALID,    "sbb",   AM_E, OT_B, AM_G, OT_B},
   /* 19 */ {VALID,    "sbb",   AM_E, OT_V, AM_G, OT_V},
   /* 1a */ {VALID,    "sbb",   AM_G, OT_B, AM_E, OT_B},
   /* 1b */ {VALID,    "sbb",   AM_G, OT_V, AM_E, OT_V},
   /* 1c */ {VALID,    "sbb",     AL,    0, AM_I, OT_B},
   /* 1d */ {VALID,    "sbb",    eAX,    0, AM_I, OT_V},
   /* 1e */ {VALID,    "push",   DS},
   /* 1f */ {VALID,    "pop",    DS},

   /* 20 */ {VALID,    "and",   AM_E, OT_B, AM_G, OT_B},
   /* 21 */ {VALID,    "and",   AM_E, OT_V, AM_G, OT_V},
   /* 22 */ {VALID,    "and",   AM_G, OT_B, AM_E, OT_B},
   /* 23 */ {VALID,    "and",   AM_G, OT_V, AM_E, OT_V},
   /* 24 */ {VALID,    "and",     AL,    0, AM_I, OT_B},
   /* 25 */ {VALID,    "and",    eAX,    0, AM_I, OT_V},
   /* 26 */ {VALID | PREFIX, LNULL, ES},
   /* 27 */ {VALID,    "daa"},
   /* 28 */ {VALID,    "sub",   AM_E, OT_B, AM_G, OT_B},
   /* 29 */ {VALID,    "sub",   AM_E, OT_V, AM_G, OT_V},
   /* 2a */ {VALID,    "sub",   AM_G, OT_B, AM_E, OT_B},
   /* 2b */ {VALID,    "sub",   AM_G, OT_V, AM_E, OT_V},
   /* 2c */ {VALID,    "sub",     AL,    0, AM_I, OT_B},
   /* 2d */ {VALID,    "sub",    eAX,    0, AM_I, OT_V},
   /* 2e */ {VALID | PREFIX, LNULL, CS},
   /* 2f */ {VALID,    "das"},

   /* 30 */ {VALID,    "xor",   AM_E, OT_B, AM_G, OT_B},
   /* 31 */ {VALID,    "xor",   AM_E, OT_V, AM_G, OT_V},
   /* 32 */ {VALID,    "xor",   AM_G, OT_B, AM_E, OT_B},
   /* 33 */ {VALID,    "xor",   AM_G, OT_V, AM_E, OT_V},
   /* 34 */ {VALID,    "xor",     AL,    0, AM_I, OT_B},
   /* 35 */ {VALID,    "xor",    eAX,    0, AM_I, OT_V},
   /* 36 */ {VALID | PREFIX, LNULL, SS},
   /* 37 */ {VALID,    "aaa"},
   /* 38 */ {VALID,    "cmp",   AM_E, OT_B, AM_G, OT_B},
   /* 39 */ {VALID,    "cmp",   AM_E, OT_V, AM_G, OT_V},
   /* 3a */ {VALID,    "cmp",   AM_G, OT_B, AM_E, OT_B},
   /* 3b */ {VALID,    "cmp",   AM_G, OT_V, AM_E, OT_V},
   /* 3c */ {VALID,    "cmp",     AL,    0, AM_I, OT_B},
   /* 3d */ {VALID,    "cmp",    eAX,    0, AM_I, OT_V},
   /* 3e */ {VALID | PREFIX, LNULL, DS},
   /* 3f */ {VALID,    "aas"},

   /* 40 */ {VALID | REXPRE, "inc",    eAX},    // REX.
   /* 41 */ {VALID | REXPRE, "inc",    eCX},    // REX.   B
   /* 42 */ {VALID | REXPRE, "inc",    eDX},    // REX.  X
   /* 43 */ {VALID | REXPRE, "inc",    eBX},    // REX.  XB
   /* 44 */ {VALID | REXPRE, "inc",    eSP},    // REX. R
   /* 45 */ {VALID | REXPRE, "inc",    eBP},    // REX. R B
   /* 46 */ {VALID | REXPRE, "inc",    eSI},    // REX. RX
   /* 47 */ {VALID | REXPRE, "inc",    eDI},    // REX. RXB
   /* 48 */ {VALID | REXPRE, "dec",    eAX},    // REX.W
   /* 49 */ {VALID | REXPRE, "dec",    eCX},    // REX.W  B
   /* 4a */ {VALID | REXPRE, "dec",    eDX},    // REX.W X
   /* 4b */ {VALID | REXPRE, "dec",    eBX},    // REX.W XB
   /* 4c */ {VALID | REXPRE, "dec",    eSP},    // REX.WR
   /* 4d */ {VALID | REXPRE, "dec",    eBP},    // REX.WR B
   /* 4e */ {VALID | REXPRE, "dec",    eSI},    // REX.WRX
   /* 4f */ {VALID | REXPRE, "dec",    eDI},    // REX.WRXB

   /* 50 */ {VALID,    "push",   eAX},
   /* 51 */ {VALID,    "push",   eCX},
   /* 52 */ {VALID,    "push",   eDX},
   /* 53 */ {VALID,    "push",   eBX},
   /* 54 */ {VALID,    "push",   eSP},
   /* 55 */ {VALID,    "push",   eBP},
   /* 56 */ {VALID,    "push",   eSI},
   /* 57 */ {VALID,    "push",   eDI},
   /* 58 */ {VALID,    "pop",    eAX},
   /* 59 */ {VALID,    "pop",    eCX},
   /* 5a */ {VALID,    "pop",    eDX},
   /* 5b */ {VALID,    "pop",    eBX},
   /* 5c */ {VALID,    "pop",    eSP},
   /* 5d */ {VALID,    "pop",    eBP},
   /* 5e */ {VALID,    "pop",    eSI},
   /* 5f */ {VALID,    "pop",    eDI},

   /* 60 */ {VALID | OPSIZE, "pushad"},
   /* 61 */ {VALID | OPSIZE,  "popad"},
   /* 62 */ {VALID,    "bound", AM_G, OT_V, AM_M, OT_A},
   /* 63 */ {VALID,     "arpl", AM_E, OT_W, AM_E, AM_G},
   /* 64 */ {VALID | PREFIX, LNULL, FS},
   /* 65 */ {VALID | PREFIX, LNULL, GS},
   /* 66 */ {VALID | PREFIX, LNULL, OS},
   /* 67 */ {VALID | PREFIX, LNULL, AS},
   /* 68 */ {VALID,    "push",  AM_I, OT_V},
   /* 69 */ {VALID,    "imul",  AM_G, OT_V, AM_E, OT_V, AM_I, OT_V},
   /* 6a */ {VALID,    "push",  AM_I, OT_B},
   /* 6b */ {VALID,    "imul",  AM_G, OT_V, AM_E, OT_V, AM_I, OT_B},
   /* 6c */ {VALID,    "insb"},
   /* 6d */ {VALID | OPSIZE,  "insd", 0, 0, 0, 0, 0, 0, 2},
   /* 6e */ {VALID,    "outsb", 0},
   /* 6f */ {VALID | OPSIZE, "outsd", 0, 0, 0, 0, 0, 0, 3},

   /* 70 */ {VALID,    "jo",     AM_J, OT_B},
   /* 71 */ {VALID,    "jno",    AM_J, OT_B},
   /* 72 */ {VALID,    "jb",     AM_J, OT_B},
   /* 73 */ {VALID,    "jnb",    AM_J, OT_B},
   /* 74 */ {VALID,    "jz",     AM_J, OT_B},
   /* 75 */ {VALID,    "jnz",    AM_J, OT_B},
   /* 76 */ {VALID,    "jbe",    AM_J, OT_B},
   /* 77 */ {VALID,    "jnbe",   AM_J, OT_B},
   /* 78 */ {VALID,    "js",     AM_J, OT_B},
   /* 79 */ {VALID,    "jns",    AM_J, OT_B},
   /* 7a */ {VALID,    "jp",     AM_J, OT_B},
   /* 7b */ {VALID,    "jnp",    AM_J, OT_B},
   /* 7c */ {VALID,    "jl",     AM_J, OT_B},
   /* 7d */ {VALID,    "jnl",    AM_J, OT_B},
   /* 7e */ {VALID,    "jle",    AM_J, OT_B},
   /* 7f */ {VALID,    "jnle",   AM_J, OT_B},

   /* 80 */ {SUBTB,(char *)g1_t, AM_E, OT_B, AM_I, OT_B, 0, 0, OC_MASK, 8},
   /* 81 */ {SUBTB,(char *)g1_t, AM_E, OT_V, AM_I, OT_V, 0, 0, OC_MASK, 8},
   /* 82 */ {SUBTB,(char *)g1_t, AM_E, OT_V, AM_I, OT_B, 0, 0, OC_MASK, 8},
   /* 83 */ {SUBTB,(char *)g1_t, AM_E, OT_V, AM_I, OT_B, 0, 0, OC_MASK, 8},
   /* 84 */ {VALID,    "test",   AM_E, OT_B, AM_G, OT_B},
   /* 85 */ {VALID,    "test",   AM_E, OT_V, AM_G, OT_V},
   /* 86 */ {VALID,    "xchg",   AM_E, OT_B, AM_G, OT_B},
   /* 87 */ {VALID,    "xchg",   AM_E, OT_V, AM_G, OT_V},
   /* 88 */ {VALID,     "mov",   AM_E, OT_B, AM_G, OT_B},
   /* 89 */ {VALID,     "mov",   AM_E, OT_V, AM_G, OT_V},
   /* 8a */ {VALID,     "mov",   AM_G, OT_B, AM_E, OT_B},
   /* 8b */ {VALID,     "mov",   AM_G, OT_V, AM_E, OT_V},
   /* 8c */ {VALID,     "mov",   AM_E, OT_W, AM_S, OT_W},
   /* 8d */ {VALID,     "lea",   AM_G, OT_V, AM_M},
   /* 8e */ {VALID,     "mov",   AM_S, OT_W, AM_E, OT_W},
   /* 8f */ {VALID,     "pop",   AM_E, OT_V},

   /* 90 */ {VALID,     "nop"},
   /* 91 */ {VALID,    "xchg",   eCX},
   /* 92 */ {VALID,    "xchg",   eDX},
   /* 93 */ {VALID,    "xchg",   eBX},
   /* 94 */ {VALID,    "xchg",   eSP},
   /* 95 */ {VALID,    "xchg",   eBP},
   /* 96 */ {VALID,    "xchg",   eSI},
   /* 97 */ {VALID,    "xchg",   eDI},
   /* 98 */ {VALID | OPSIZE, "cbw", 0, 0, 0, 0, 0, 0, 13},   //STJ  cbw/cwde/cdqe
   /* 99 */ {VALID | OPSIZE, "cwd", 0, 0, 0, 0, 0, 0, 4},    //     cwd/cdq/cqo
   /* 9a */ {VALID,    "call",   AM_A, OT_P},
   /* 9b */ {VALID,    "wait"},                              //     wait/fwait
   /* 9c */ {VALID | OPSIZE, "pushfd", 0, 0, 0, 0, 0, 0, 5},
   /* 9d */ {VALID | OPSIZE,  "popfd", 0, 0, 0, 0, 0, 0, 6},
   /* 9e */ {VALID,    "sahf"},
   /* 9f */ {VALID,    "lahf"},

   /* a0 */ {VALID,    "mov",      AL,    0, AM_O, OT_V},
   /* a1 */ {VALID,    "mov",     eAX,    0, AM_O, OT_V},
   /* a2 */ {VALID,    "mov",    AM_O, OT_V,  AL},
   /* a3 */ {VALID,    "mov",    AM_O, OT_V, EAX},
   /* a4 */ {VALID,    "movsb"},
   /* a5 */ {VALID | OPSIZE, "movsd", 0, 0, 0, 0, 0, 0, 7},
   /* a6 */ {VALID,    "cmpsb"},
   /* a7 */ {VALID | OPSIZE, "cmpsd", 0, 0, 0, 0, 0, 0, 8},
   /* a8 */ {VALID,    "test",     AL,    0, AM_I, OT_B},
   /* a9 */ {VALID,    "test",    eAX,    0, AM_I, OT_V},
   /* aa */ {VALID,    "stosb"},
   /* ab */ {VALID | OPSIZE, "stosd", 0, 0, 0, 0, 0, 0,  9},
   /* ac */ {VALID,    "lodsb"},
   /* ad */ {VALID | OPSIZE, "lodsd", 0, 0, 0, 0, 0, 0, 10},
   /* ae */ {VALID,    "scasb"},
   /* af */ {VALID | OPSIZE, "scasd", 0, 0, 0, 0, 0, 0, 11},

   /* b0 */ {VALID,    "mov",    AM_I, OT_B, AL},
   /* b1 */ {VALID,    "mov",    AM_I, OT_B, CL},
   /* b2 */ {VALID,    "mov",    AM_I, OT_B, DL},
   /* b3 */ {VALID,    "mov",    AM_I, OT_B, BL},
   /* b4 */ {VALID,    "mov",    AM_I, OT_B, AH},
   /* b5 */ {VALID,    "mov",    AM_I, OT_B, CH},
   /* b6 */ {VALID,    "mov",    AM_I, OT_B, DH},
   /* b7 */ {VALID,    "mov",    AM_I, OT_B, BH},
   /* b8 */ {VALID,    "mov",     eAX,    0, AM_I, OT_V},
   /* b9 */ {VALID,    "mov",     eCX,    0, AM_I, OT_V},
   /* ba */ {VALID,    "mov",     eDX,    0, AM_I, OT_V},
   /* bb */ {VALID,    "mov",     eBX,    0, AM_I, OT_V},
   /* bc */ {VALID,    "mov",     eSP,    0, AM_I, OT_V},
   /* bd */ {VALID,    "mov",     eBP,    0, AM_I, OT_V},
   /* be */ {VALID,    "mov",     eSI,    0, AM_I, OT_V},
   /* bf */ {VALID,    "mov",     eDI,    0, AM_I, OT_V},

   /* c0 */ {SUBTB,(char *)g2_t, AM_E, OT_B, AM_I, OT_B, 0, 0, OC_MASK, 8},   //STJ
   /* c1 */ {SUBTB,(char *)g2_t, AM_E, OT_V, AM_I, OT_B, 0, 0, OC_MASK, 8},   //STJ
   /* c2 */ {VALID,    "ret near", AM_I, OT_W},
   /* c3 */ {VALID,    "ret near"},
   /* c4 */ {VALID,    "les",    AM_G, OT_V, AM_M, OT_P},
   /* c5 */ {VALID,    "lds",    AM_G, OT_V, AM_M, OT_P},
   /* c6 */ {VALID,    "mov",    AM_E, OT_B, AM_I, OT_B},
   /* c7 */ {VALID,    "mov",    AM_E, OT_V, AM_I, OT_V},
   /* c8 */ {VALID,    "enter",  AM_I, OT_W, AM_I, OT_B},
   /* c9 */ {VALID,    "leave"},
   /* ca */ {VALID,    "ret far",AM_I, OT_W},
   /* cb */ {VALID,    "ret far"},
   /* cc */ {VALID,    "int",    THREE},
   /* cd */ {VALID,    "int",    AM_I, OT_B},
   /* ce */ {VALID,    "into"},
   /* cf */ {VALID,    "iret"},

   /* d0 */ {SUBTB,(char *)g2_t, AM_E, OT_B,  1, 0, 0, 0, OC_MASK, 8},   //STJ
   /* d1 */ {SUBTB,(char *)g2_t, AM_E, OT_V,  1, 0, 0, 0, OC_MASK, 8},   //STJ
   /* d2 */ {SUBTB,(char *)g2_t, AM_E, OT_B, CL, 0, 0, 0, OC_MASK, 8},   //STJ
   /* d3 */ {SUBTB,(char *)g2_t, AM_E, OT_V, CL, 0, 0, 0, OC_MASK, 8},   //STJ
   /* d4 */ {VALID,    "aam",    AM_I, OT_B},
   /* d5 */ {VALID,    "aad",    AM_I, OT_B},
   /* d6 */ {VALID,    "salc"},         // Undocumented: Set AL from Carry //STJ
   /* d7 */ {VALID,    "xlat"},
   /* d8 */ {SUBTB, (char *)d8_t, 0, 0, 0, 0, 0, 0, MOD_MASK, 4},
   /* d9 */ {SUBTB, (char *)d9_t, 0, 0, 0, 0, 0, 0, MOD_MASK, 4},
   /* da */ {SUBTB, (char *)da_t, 0, 0, 0, 0, 0, 0, MOD_MASK, 4},
   /* db */ {SUBTB, (char *)db_t, 0, 0, 0, 0, 0, 0, MOD_MASK, 4},
   /* dc */ {SUBTB, (char *)dc_t, 0, 0, 0, 0, 0, 0, MOD_MASK, 4},
   /* dd */ {SUBTB, (char *)dd_t, 0, 0, 0, 0, 0, 0, MOD_MASK, 4},
   /* de */ {SUBTB, (char *)de_t, 0, 0, 0, 0, 0, 0, MOD_MASK, 4},
   /* df */ {SUBTB, (char *)df_t, 0, 0, 0, 0, 0, 0, MOD_MASK, 4},

   /* e0 */ {VALID,    "loopne", AM_J, OT_B},
   /* e1 */ {VALID,    "loope",  AM_J, OT_B},
   /* e2 */ {VALID,    "loop",   AM_J, OT_B},
   /* e3 */ {VALID | ADSIZE, "jecxz",    AM_J, OT_B, 0, 0, 0, 0, 12},
   /* e4 */ {VALID,    "in",      AL,     0,  AM_I, OT_B},
   /* e5 */ {VALID,    "in",     eAX,     0,  AM_I, OT_B},
   /* e6 */ {VALID,    "out",    AM_I, OT_B,    AL},
   /* e7 */ {VALID,    "out",    AM_I, OT_B,   eAX},
   /* e8 */ {VALID,    "call",   AM_J, OT_V},
   /* e9 */ {VALID,    "jmp",    AM_J, OT_V},
   /* ea */ {VALID,    "jmp",    AM_A, OT_P},
   /* eb */ {VALID,    "jmp",    AM_J, OT_B},
   /* ec */ {VALID,    "in",       AL,    0,  DX},
   /* ed */ {VALID,    "in",      eAX,    0,  DX},
   /* ee */ {VALID,    "out",      DX,    0,  AL},
   /* ef */ {VALID,    "out",      DX,    0, eAX},

   /* f0 */ {VALID | PREFIX, "lock"},
   /* f1 */ {VALID,    "int1"},         // AMD //STJ
   /* f2 */ {VALID | PREFIX, "repne"},
   /* f3 */ {VALID | PREFIX, "rep"},
   /* f4 */ {VALID,    "hlt"},
   /* f5 */ {VALID,    "cmc"},
   /* f6 */ {SUBTB,(char *)g3a_t, AM_E, OT_B, 0, 0, 0, 0, OC_MASK, 7},
   /* f7 */ {SUBTB,(char *)g3b_t, AM_E, OT_V, 0, 0, 0, 0, OC_MASK, 7},
   /* f8 */ {VALID,    "clc"},
   /* f9 */ {VALID,    "stc"},
   /* fa */ {VALID,    "cli"},
   /* fb */ {VALID,    "sti"},
   /* fc */ {VALID,    "cld"},
   /* fd */ {VALID,    "std"},
   /* fe */ {SUBTB, (char *)g4_t, 0, 0, 0, 0, 0, 0, OC_MASK, 2},
   /* ff */ {SUBTB, (char *)g5_t, 0, 0, 0, 0, 0, 0, OC_MASK, 7}
};


/* OPSIZE/ADSIZE flag & operand_size/address_size sensitivity */
static char *opsize_mnemonics[] =
{
   "pusha", "popa",  "insw",  "outsw",
   "cwd",   "pushf", "popf",  "movsw",
   "cmpsw", "stosw", "lodsw", "scasw",
   "jcxz",  "cbw"
};


/* operand size vs. [p_os] & [OT] */
static int op_sz[2][OT_NUM] =
{
   { 0, 1, 2, 4, 6, 8, 6, 4, 2, 4, 8},
   { 0, 1, 1, 4, 4, 8, 6, 2, 2, 4, 8}
};

static PREFIX_T prefix;
static char     t_buf[128];
char            preStr[32] = "";
int             ooride     = 0;


/******************************************************************************/
void show_inst(INSTRUCTION * p)
{
   int i, j;

   OptVMsg("\n Show Instruction Structure\n");
   OptVMsg("  - type  %x", p->type);

   for (i = 0; i < 8; i++)              //STJ
   {
      if ((p->type >> (8 + i)) & 0x1)
      {
         OptVMsg(" <%s>", optypestr[i]);
      }
   }
   OptVMsg("\n  - mnem  <%s>\n", p->mnemonic);

   for (j = 0; j < MAX_OPERANDS; j++)
   {
      if (p->par[j].meth > 0)
      {
         OptVMsg("  - meth %d <%s> type %d <%s>\n",
                 p->par[j].meth, mdis[p->par[j].meth],
                 p->par[j].type, mdis[p->par[j].type]);
      }
   }
   OptVMsg("  - mask %x", p->mask);
   OptVMsg("  - leng %x\n", p->length);

   fflush(stdout);
}


/******************************************************************************
 * Function Name: rm_mod_byte_present
 *
 * Description:   Detects RM/mod & SIB presents
 *
 * Output:
 *     Return:    TRUE if RM/mod byte present
 *     Parms:     *sib_byte :  value of the SIB byte,
 *                *sib_flag :  TRUE if the sib present
 ******************************************************************************/
static BOOL rm_mod_byte_present(
                               char         *mod_byte,   /* next instruction byte */
                               INSTRUCTION  *instruction,
                               char         *sib_byte,
                               BOOL         *sib_flag)
{
   int  ox;                             /* operand index     */
   int  am;                             /* addressing method */
   BOOL rc = FALSE;                     /* return code       */

   MVMsg(gc_mask_Show_Disasm,(" => rm_mod_byte_present\n"));

   *sib_flag = FALSE;
   for (ox = 0; ox < MAX_OPERANDS; ox++)
   {
      am = instruction->par[ox].meth;
      MVMsg(gc_mask_Show_Disasm,(" am = %d\n", am));
      if (am == 0) break;
      if ((am == AM_G) || (am == AM_E) || (am == AM_D) || (am == AM_C) ||
          (am == AM_V) || (am == AM_P) ||
          (am == AM_M) || (am == AM_R) || (am == AM_S) ||
          (am == AM_T) || (am == ST) || (am == ST_NUM) )
      {
         rc = TRUE;                     /* modr/m byte */

         /* sib byte ?? */
         if (((am == AM_E) || (am == AM_M)) &&
             (V_MOD(*mod_byte) != 3) && (V_RM(*mod_byte) == 4))
         {
            *sib_byte = *(mod_byte + 1);
            *sib_flag = TRUE;
         }
      }
   }
   return rc;
}


/******************************************************************************
 * Function Name: operand_value
 *
 * Description:   Returns operand values of data(1st parm).
 *                operand(4th parm) points to OPERAND struct of
 *                INSTRUCTION struct.
 * Output:
 *   parms:
 *      aux_value    - second value of operand(e.g quad data case)
 *      string       - ASCII string to display,
 *      aux_string   - second string to display,
 *      seg_string   - segment override string to display if any,
 *      format       - value of the display formet(e.g. F_H),
 *      data_size    - size of operand data in bytes,
 *   return value:
 *      return       - value of the operand.
 */
static long operand_value(
                         char    * data,   /* 1st operand byte  */
                         char    * mod, /* RM/mod byte       */
                         char      sib, /* SIB byte          */
                         OPERAND * optr,   /* operand ptr       */
                         long    * aux_value,
                         char   ** string,
                         char   ** aux_string,
                         char   ** seg_string,
                         int     * format,
                         int     * data_size)
{
   static char *gp_reg[5][8] =
   {
      { "al",   "cl",   "dl",   "bl",   "ah",   "ch",   "dh",    "bh"},
      { "ax",   "cx",   "dx",   "bx",   "sp",   "bp",   "si",   "di"},
      { "eax",  "ecx",  "edx",  "ebx",  "esp",  "ebp",  "esi",  "edi"},
      { "mm0",  "mm1",  "mm2",  "mm3",  "mm4",  "mm5",  "mm6",  "mm7"},
      { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"}
   };
   static char *reg_name[] =
   {
      LNULL,
      "al",  "cl",  "dl",  "bl",
      "ah",  "ch",  "dh",  "bh",
      "ax",  "cx",  "dx",  "bx",
      "sp",  "bp",  "si",  "di",
      "eax", "ecx", "edx", "ebx",
      "esp", "ebp", "esi", "edi",
      "es",  "cs",  "ss",  "ds",
      "fs",  "gs",  "st",  "st(",
      "3"
   };
   static char *seg_reg[] =
   {
      "es", "cs", "ss", "ds", "fs", "gs"
   };
   static char *seg_overide[] =
   {
      LNULL,
      "es:", "cs:", "ss:", "ds:", "fs:", "gs:"
   };
   static char *reg_16bit[] =
   {
      "bx+si", "bx+di", "bp+si", "bp+di",
      "si",    "di",    "bp",    "bx"
   };

   long value = 0;                      /* operand value                       */
   BOOL data_follows = FALSE;           /* data follows instruction            */
   int  rtype;                          /* 0 = byte, 1 = word, 2 = double word */
   BOOL sib_present = FALSE;            /* SIB byte                            */
   int  reg_size;                       /* 16-bit reg = 1, 32-bit reg = 2      */
   PREFIX_T * p = &prefix;

   MVMsg(gc_mask_Show_Disasm,(" => operand_value\n"));

   *format = p->p_os ? F_H4 : F_H;
   *string = "";                        // #EMP# Was: *string = LNULL;
   *aux_string = "";                    // #EMP#
   *seg_string = "";                    // #EMP#
   *aux_value = 0;                      // #EMP#
   *data_size = 0;

   switch (optr->meth)
   {
   case AM_A:
      data_follows = TRUE;
      break;

   case AM_C:
      value = V_REG(*mod);
      *string = "cr";
      *format = F_SD;
      break;

   case AM_D:
      value = V_REG(*mod);
      *string = "dr";
      *format = F_SD;
      break;

   case AM_M:
   case AM_E:
      *seg_string = seg_overide[SEG_PREFIX(prefix)];
      switch (V_MOD(*mod))
      {
      case 0:
         if (p->p_as)
         {                              /* 16-bit addressing mode */
            if (V_RM(*mod) == 6)
            {
               value = WORD(data);
               *data_size = 2;
               *string = "*";
               *format = F_SH;
            }
            else
            {
               *string = reg_16bit[V_RM(*mod)];
               *format = F_R;
            }
         }
         else
         {                              /* 32-bit addressing mode */
            if (V_RM(*mod) == 4) sib_present = TRUE;
            else
            {
               if (V_RM(*mod) == 5)
               {
                  value = DWORD(data);
                  *data_size = 4;
                  *string = "*";
                  *format = F_SH;
               }
               else
               {
                  *string = gp_reg[2][V_RM(*mod)];
                  *format = F_R;
               }
            }
         }
         break;

      case 1:
         if (p->p_as)
         {                              /* 16-bit addressing mode */
            *string = reg_16bit[V_RM(*mod)];
            *format = F_DR;
            value = *data & 0xff;
            if (value & 0x80)           /* extend sign  */
               value |= 0xffffff00;
            *data_size = 1;
         }
         else
         {                              /* 32-bit addressing mode */
            if (V_RM(*mod) == 4)
            {
               sib_present = TRUE;
               *format = F_D;
            }
            else
            {
               *string = gp_reg[2][V_RM(*mod)];
               *format = F_DR;
            }
            value = *data & 0xff;
            if (value & 0x80)           /* extend sign  */
               value |= 0xffffff00;
            *data_size = 1;
         }
         break;

      case 2:
         if (p->p_as)
         {                              /* 16-bit addressing mode */
            *string = reg_16bit[V_RM(*mod)];
            *format = F_DR;
            value = WORD(data);
            *data_size = 2;
         }
         else
         {                              /* 32-bit addressing mode */
            if (V_RM(*mod) == 4)
            {
               sib_present = TRUE;
               *format = F_D;
            }
            else
            {
               *string = gp_reg[2][V_RM(*mod)];
               *format = F_DR;
            }
            value = DWORD(data);
            *data_size = 4;
         }
         break;

      case 3:
      default:
         rtype = REG_TYPE(op_sz[p->p_os][optr->type - MIN_OT]);
         *string = gp_reg[rtype][V_RM(*mod)];
         *format = F_S;
         break;
      }

      if (sib_present)
      {
         if ((V_MOD(*mod) == 0) && (V_BASE(sib) == 5))
         {
            value = DWORD(data);
            *data_size = 4;
            *format = F_D;
         }
         else
         {
            *string = gp_reg[2][V_BASE(sib)];
            *format = (*format == F_D) ? F_DR : F_R;
         }

         if (V_INDEX(sib) != 4)
         {
            MVMsg(gc_mask_Show_Disasm,(" bef_sib %d %s\n", *format, fdis[*format]));
            *aux_value = 1 << V_SS(sib);
            *aux_string = gp_reg[2][V_INDEX(sib)];
            if (*format == F_D) *format = F_HR;
            else
            {
               if (*format == F_DR) *format = F_DRR;
               else                      *format = F_RR;
            }
            MVMsg(gc_mask_Show_Disasm,(" aft_sib %d %s\n", *format, fdis[*format]));
         }
      }
      break;

      /* register operands */
   case AM_G:
      rtype = REG_TYPE(op_sz[p->p_os][optr->type - MIN_OT]);
      *string = gp_reg[rtype][V_REG(*mod)];
      *format = F_S;
      break;

   case AM_V:
      *string = gp_reg[4][V_REG(*mod)];
      *format = F_S;
      break;

   case AM_P:
      *string = gp_reg[3][V_REG(*mod)];
      *format = F_S;
      break;

   case AM_I:
      data_follows = TRUE;
      *format = p->p_os ? F_H4 : F_HREL;
      break;

   case AM_J:
      data_follows = TRUE;
      *format = p->p_os ? F_SA : F_HA;
      break;

   case AM_O:
      data_follows = TRUE;
      *format = p->p_os ? F_SH4 : F_SH;
      *string = "*";                    /* deref a ptr */
      break;

   case AM_R:
      *string = gp_reg[2][V_RM(*mod)];  /* assume OT_D follows */
      *format = F_S;
      break;

   case AM_S:
      *string = seg_reg[V_REG(*mod)];
      *format = F_S;
      break;

   case AM_T:
      value = V_REG(*mod);
      *string = "tr";
      *format = F_SD;
      break;

   case ST_NUM:
      /*value = V_RM(*(mod - 1)); */    /* mod points 1 byte too far */
      value = V_RM(*(mod));
      *string = reg_name[ST_NUM];
      *format = F_SDP;
      break;

   default:
      if (optr->meth <= MAX_REG_NUM)
      {
         *string = reg_name[optr->meth];
         *format = F_S;
      }
      else
      {
         if ((eAX <= optr->meth) && (optr->meth <= eDI))
         {
            reg_size = p->p_os ? 1      /*16-bit*/ : 2   /*32-bit*/;
            *string = gp_reg[reg_size][optr->meth % 8];
            *format = F_S;
         }
      }
      break;
   }

   if (data_follows)
   {
      *data_size = op_sz[p->p_os][optr->type - MIN_OT];
      switch (optr->type)
      {
      case OT_A:        /* 2 16/32 bit operands(p_os)(BOUND) */
         value = p->p_os ? WORD(data) : DWORD(data);
         *aux_value = p->p_os ? WORD(data + 4) : DWORD(data + 4);
         *format = p->p_os ? F_H44 : F_H88;
         break;

      case OT_B:        /* byte */
         value = *data & 0xff;
         /** RJU 7/6/98: fix relative jump. add sign extend. ??? */
         /* need to test if this breaks anyone else **/
         if (value >= 128) value = value - 256;

         if (*format == F_HREL)
            *format = F_H;
         break;

      case OT_C:     /* byte or word(p_os) */
         value = p->p_os ? WORD(data) : *data & 0xff;
         break;

      case OT_D:     /* double word */
         value = DWORD(data);
         break;

      case OT_P:     /* 32 or 48-bit pointer(p_os) */
         value = p->p_os ? WORD(data + 2) : WORD(data + 4);
         *aux_value = p->p_os ? WORD(data) : DWORD(data);

         *format = p->p_os ? F_H44 : F_H48;
         break;

      case OT_Q:     /* quad word */
         value = DWORD(data);
         *aux_value = DWORD(data + 4);
         break;

      case OT_V:     /* word or double word(p_os)  #EMP# can be qword in 64-bit mode */
         value = p->p_os ? WORD(data) : DWORD(data);
         break;

      case OT_W:     /* word */
         value = WORD(data);
         break;

      case OT_SI:    /* short integer - 4 bytes */
         value = DWORD(data);
         break;

      case OT_LI:    /* long integer - 8 bytes */
         value = DWORD(data);
         *aux_value = DWORD(data + 4);
         *format = F_H88;
         break;

      case OT_S:     /* 6-byte pseudo-descriptor */
         value = 0;
         break;

      // #EMP# Need to handle these
      // case OT_DQ:  Double Quadword  (16 bytes)
      // case OT_PI:  Quadword MMX register  (8 bytes)
      // case OT_PS:  128-bit packed single precision floating point  (16 bytes)
      // case OT_SS:  Scalar element of a 128-bit packed single precision floating point  (? bytes)
      default:
         value = 0;
         break;
      }
   }
   return value;
}


/******************************************************************************/
/* Function Name: get_instruction_length                                      */
/*                                                                            */
/* Output:        instruction length                                          */
/******************************************************************************/
static int get_instruction_length(
                                 char        *data,   /* ptr to instruction         */
                                 char        *modRM,   /* modR/M byte or 1st operand */
                                 INSTRUCTION *inst,
                                 BOOL         mod_present)   /* RM/mod present             */
{
   int length;
   int am;                              /* addressing method    */
   int ot;                              /* operand_type         */
   int ox;                              /* operand index        */

   MVMsg(gc_mask_Show_Disasm,(" => get_instruction_length\n"));

   length = (int)(modRM - data);        /* prefixes counted separately */

   if (mod_present)
   {
      length++;
      MVMsg(gc_mask_Show_Disasm,(" => MOD BYTE PRESENT\n"));
   }

   for (ox = 0; ox < MAX_OPERANDS; ox++)
   {
      am = inst->par[ox].meth;
      if (am == 0)
      {
         /* */
         if ( (ox == 0) && ( (*data & 0xf8) == 0xd8) )
         {
            length++;
         }
         break;
      }
      ot = inst->par[ox].type;

      /* add up length due to EA generation */
      if ((am == AM_E) || (am == AM_M))
      {
         if (prefix.p_as == FALSE)
         {                              /* 32 bit */
            if ((V_MOD(*modRM) != 3) && (V_RM(*modRM) == 4))
            {
               length++;                /* SIB byte */
               if ((V_MOD(*modRM) == 0) && (V_BASE(*(modRM + 1)) == 5))
                  length += 4;          /* Immediate (vs. EBP) */
            }

            if (((V_MOD(*modRM) == 0) && (V_RM(*modRM) == 5)) || (V_MOD(*modRM) == 2))
               length += 4;             /* disp32 */
            if (V_MOD(*modRM) == 1) length++;   /* disp8 */
         }

         else
         {                              /* 16 bit */
            if (((V_MOD(*modRM) == 0) && (V_RM(*modRM) == 6)) || (V_MOD(*modRM) == 2))
               length += 2;             /* disp16 */
            if (V_MOD(*modRM) == 1) length++;   /* disp8 */
         }
      }
      else
      {
         if (ot)
         {
            if ((am == AM_I) || (am == AM_J) || (am == AM_A))
            {
               length += op_sz[prefix.p_os][ot - MIN_OT];
            }
            else if (am == AM_O)
            {
               length += prefix.p_as ? 2 : 4;
            }
         }
      }
   }

   if ( ((unsigned char)*data == 0xdf) && ((unsigned char)*(data + 1) ) )
   {
      length++;
   }

   return length;
}


/******************************************************************************
 * Function Name: set_prefix_value
 *
 * Description:   Sets prefix_value TRUE. Increments p_valid.
 ******************************************************************************/
static void set_prefix_value(char code)
{
   int i;
   int prev = 0;

   MVMsg(gc_mask_Show_Disasm,(" => set_prefix_value\n"));

   i = (int)code & 0xff;

   prefix.p_valid++;

   switch (i)
   {
   case 0x26:
      prefix.p_es = TRUE;
      ooride = 1;
      strcat(preStr, " ES:");
      break;

   case 0x2e:
      prefix.p_cs = TRUE;
      ooride = 1;
      strcat(preStr, " CS:");
      break;

   case 0x36:
      prefix.p_ss = TRUE;
      ooride = 1;
      strcat(preStr, " SS:");
      break;

   case 0x3e:
      prefix.p_ds = TRUE;
      ooride = 1;
      strcat(preStr, " DS:");
      break;

   case 0x64:
      prefix.p_fs = TRUE;
      ooride = 1;
      strcat(preStr, " FS:");
      break;

   case 0x65:
      prefix.p_gs = TRUE;
      ooride = 1;
      strcat(preStr, " GS:");
      break;

   case 0x66:
      prev = prefix.p_os;
      prefix.p_os = !prefix.p_os;
      break;

   case 0x67:
      prev = prefix.p_as;
      prefix.p_as = !prefix.p_as;
      break;

   case 0xf0: prefix.p_lock  = TRUE;  break;
   case 0xf2: prefix.p_repne = TRUE;  break;
   case 0xf3: prefix.p_rep   = TRUE;  break;

   case 0x40:
   case 0x41:
   case 0x42:
   case 0x43:
   case 0x44:
   case 0x45:
   case 0x46:
   case 0x47:
   case 0x48:
   case 0x49:
   case 0x4A:
   case 0x4B:
   case 0x4C:
   case 0x4D:
   case 0x4E:
   case 0x4F:
      prefix.p_rex     = TRUE;
      prefix.p_rexType = i & 0x0F;
      ooride = 1;
      strcat(preStr, rexprefix[prefix.p_rexType]);
      break;

   default:   break;
   }
   pre_tot[i][prev]++;                  /* stats */
}


/****************************************************
 * Function Name: show_prefix_cnts
 *
 * Description:   returns cumulative prefix usage
 *
 * Output:        via parms 2 & 3
 ****************************************************/
void show_prefix_cnt(char code, int * p01, int * p10)
{
   MVMsg(gc_mask_Show_Disasm,(" => show_prefix_cnt\n"));

   *p01 = pre_tot[(int)code & 0xff][0];
   *p10 = pre_tot[(int)code & 0xff][1];
}


/****************************************************
 * Function Name: clear_prefix_values
 *
 * Description:   PREFIX values to FALSE
 ****************************************************/
static void clear_prefix_values(void)
{
   MVMsg(gc_mask_Show_Disasm,(" => clear_prefix_values\n"));

   if (prefix.p_valid)
   {
      prefix.p_os    = FALSE;
      prefix.p_as    = FALSE;
      prefix.p_es    = FALSE;
      prefix.p_cs    = FALSE;
      prefix.p_ss    = FALSE;
      prefix.p_ds    = FALSE;
      prefix.p_fs    = FALSE;
      prefix.p_gs    = FALSE;
      prefix.p_lock  = FALSE;
      prefix.p_repne = FALSE;
      prefix.p_rep   = FALSE;
      prefix.p_valid = 0;
   }
}


/****************************************************
 * Function Name: decode_instruction
 *
 * Description:   decodes 1 Intel 386 instruction.
 *
 * Output:
 *   Return value - instruction length
 *   *buf         - disassembled output
 ****************************************************/
static int decode_instruction(
                             char        * data,   /* instruction(without prefixes)  */
                             char        * modRM,   /* modR/M byte or first operand   */
                             INSTRUCTION * instr,
                             uint64        addr,   /* virtual address/offset         */   //STJ64
                             char        * buf)
{
   static char *m_format_1 = "%s ";
   static char *m_format_2 = "%-7s ";
   char * m_format;                     /* prefix dependent format   */
   char * str     = "";                 /* operand ASCII string      */
   char * sec_str = "";                 /* second string             */
   char * segment_str = "";             /* segment override string   */
   int    of;                           /* display format - e.g. F_R */
   long   value;                        /* operand value             */
   long   sec_val;                      /* second operand value      */
   int    length;                       /* instruction length        */
   BOOL   rm_mod;                       /* RM/mod flag               */
   BOOL   sib;                          /* SIB flag                  */
   char   sib_byte;                     /* SIB byte                  */
   int    ox;                           /* operand index */
   int    op_size;                      /* operand size              */
   char * instr_start = data;           /* instruction start */

   MVMsg(gc_mask_Show_Disasm,(" => decode_instruction\n"));

   rm_mod = rm_mod_byte_present(modRM, instr, &sib_byte, &sib);

   length = get_instruction_length(data, modRM, instr, rm_mod);

   /* if dd & > c0 => length++ */

   if (gc_mask_Show_Disasm & gc.mask)
   {
      if (rm_mod) OptVMsg(" -- rm_mod %02x\n", *modRM);
      if (sib)    OptVMsg(" --    sib %02x\n", sib_byte);
      OptVMsg(" -- length %d\n", length);
   }

   if (instr->type & INVALID)
   {
      strcat(buf, INVALID_INSTRUCTION);
      OptVMsgLog(" INVALID_1: In decode_instruction\n");
      if (rm_mod) OptVMsgLog(" -- rm_mod %02x\n", *modRM);
      if (sib)    OptVMsgLog(" --    sib %02x\n", sib_byte);
      OptVMsgLog(" -- length %d\n", length);
      OptVMsgLog("   %02x %02x %02x\n",
                 (unsigned char)(*(data    )),
                 (unsigned char)(*(data + 1)),
                 (unsigned char)(*(data + 2)) );
      if (istop == 1)
      {
         panic(" INVALID_INSTRUCTION_1");
      }
   }
   else
   {
      strcat(buf, "\t");                /* alignment */
      if (prefix.p_lock)
      {
         strcat(buf, "lock ");
      }
      if (prefix.p_repne)
      {
         strcat(buf, "repne ");
      }
      if (prefix.p_rep)
      {
         strcat(buf, "rep ");
      }
      m_format = (prefix.p_lock || prefix.p_repne || prefix.p_rep) ?
                 m_format_1 : m_format_2;

      /* if mnemonic depends on p_os or p_as */
      if (((instr->type & OPSIZE) && prefix.p_os) ||
          ((instr->type & ADSIZE) && prefix.p_as))
      {
         sprintf(t_buf, m_format, opsize_mnemonics[instr->mask]);
         sprintf(OPNM, "%s", opsize_mnemonics[instr->mask]);
         strcat(buf, t_buf);
      }
      else
      {
         sprintf(t_buf, m_format, instr->mnemonic);
         sprintf(OPNM, "%s", instr->mnemonic);
         strcat(buf, t_buf);
      }

      data = modRM + rm_mod + sib;      /* move data past rm_mod & sib */

      for (ox = 0; ox < MAX_OPERANDS; ox++)
      {
         if (instr->par[ox].meth == 0) break;

         value = operand_value(data, modRM, sib_byte, &instr->par[ox],
                               &sec_val, &str, &sec_str, &segment_str, &of, &op_size);

         if (gc_mask_Show_Disasm & gc.mask)
         {
            OptVMsg("\n @ operand_value return \n");
            OptVMsg(" par[%d] meth %d %s type %d %s\n", ox,
                    instr->par[ox].meth, mdis[instr->par[ox].meth],
                    instr->par[ox].type, mdis[instr->par[ox].type]);
            OptVMsg(" values     %d %d\n",    value, sec_val);
            OptVMsg(" strings   <%s> <%s>\n", str, sec_str);
            OptVMsg(" segment_str  <%s>\n",   segment_str);
            OptVMsg(" format       %d %s\n",  of, fdis[of]);
            OptVMsg(" op_size      %d\n",     op_size);
         }

         if (ox != 0) strcat(buf, ", ");

         // Make sure all strings are good
         if (segment_str == NULL)
            segment_str = "";
         if (sec_str == NULL)
            sec_str = "";
         if (str == NULL)
            str = "";

         switch (of)
         {
         case F_H44:
            sprintf(t_buf, "%04x:%04x", value & 0xffff, sec_val & 0xffff);
            strcat(buf, t_buf);
            break;

         case F_H88:
            sprintf(t_buf, "%08x:%08x", value, sec_val);
            strcat(buf, t_buf);
            break;

         case F_H48:
            sprintf(t_buf, "%04x:%08x", value & 0xffff, sec_val);
            strcat(buf, t_buf);
            break;

         case F_S:
            sprintf(t_buf, "%s", str);
            strcat(buf, t_buf);
            break;

         case F_SD:
            sprintf(t_buf, "%s%s%d", segment_str, str, value);
            strcat(buf, t_buf);
            break;

            /* what! no * for ptr ?? */
         case F_SDP:
            sprintf(t_buf, "%s%s%d)", segment_str, str, value);
            strcat(buf, t_buf);
            break;

         case F_R:
            sprintf(t_buf, "%s[%s]", segment_str, str);
            strcat(buf, t_buf);
            break;

         case F_RR:
            if (sec_val == 1)
            {
               sprintf(t_buf, "%s[%s][%s]", segment_str, str, sec_str);
               strcat(buf, t_buf);
            }
            else
            {
               sprintf(t_buf, "%s[%s][%s*%d]", segment_str, str, sec_str, sec_val);
               strcat(buf, t_buf);
            }
            break;

         case F_HREL:                   /*     3     */
         case F_D:                      /*  2 !3     */
         case F_DR:                     /*  2 !3 4   */
         case F_DRR:                    /*  2 !3  5  */
         case F_HR:                     /*  2  3   6 */
         case F_SH:                     /* 1   3     */
         case F_SH4:                    /* 1   3     */
            if ((of == F_SH) || (of == F_SH4))
            {                           /*1*/
               sprintf(t_buf, "%s", str);
               strcat(buf, t_buf);
            }

            if ((of != F_SH) && (of != F_HREL) &&
                (of != F_SH4))
            {                           /*2*/

               sprintf(t_buf, "%s", segment_str);
               strcat(buf, t_buf);
            }

            addr += (data - instr_start);
            if (of == F_SH4) value &= 0xffff;

            if ((of == F_HREL) || (of == F_SH) || (of == F_SH4) ||
                (of == F_HR))
            {                           /*3*/
               sprintf(t_buf, "0x%x", value);
               strcat(buf, t_buf);
            }
            else
            {                           /*!3*/
               sprintf(t_buf, "%d", value);
               strcat(buf, t_buf);
            }

            if (of == F_DR)
            {                           /*4*/
               sprintf(t_buf, "[%s]", str);
               strcat(buf, t_buf);
            }
            else if (of == F_DRR)
            {                           /*5*/
               if (sec_val == 1)
               {
                  sprintf(t_buf, "[%s][%s]", str, sec_str);
                  strcat(buf, t_buf);
               }
               else
               {
                  sprintf(t_buf, "[%s][%s*%d]", str, sec_str, sec_val);
                  strcat(buf, t_buf);
               }
            }
            else if (of == F_HR)
            {                           /*6*/
               if (sec_val == 1)
               {
                  sprintf(t_buf, "[%s]", sec_str);
                  strcat(buf, t_buf);
               }
               else
               {
                  sprintf(t_buf, "[%s*%d]", sec_str, sec_val);
                  strcat(buf, t_buf);
               }
            }
            break;

         case F_HA:
         case F_SA:
            addr += value + length;

            sprintf( t_buf, "0x%x",
                     ( of == F_SA )
                     ? ((size_t)addr) & 0xffff
                     : (size_t)addr );

            strcat(buf, t_buf);

            break;

         case F_H4:
            sprintf(t_buf, "0x%x", value & 0xffff);
            strcat(buf, t_buf);
            break;

         case F_H:
         default:
            sprintf(t_buf, "0x%x", value);
            strcat(buf, t_buf);
            break;
         }
         data += op_size;
      }
   }
   if (gc_mask_Show_Disasm & gc.mask)
   {
      OptVMsg(" buf=%s\n", buf );
   }
   clear_prefix_values();
   return length;
}


/****************************************************
 * Function Name: i386_instruction
 *
 * Description:   Disassembles one Intel 386 instruction.
 *
 * Output:        * size - size of instruction in bytes.
 *                * buf  - disasm output buffer
 ****************************************************/
char * i386_instruction(
                       char * data,     /* instruction     */
                       uint64 addr,     /* address/offset  */   //STJ64
                       int  * size,
                       char * buf,
                       int    mode )    /* 0 = 16, 2 = 32, 1 = 64-bit segment */
{
   INSTRUCTION * inst         = 0;
   INSTRUCTION * tbl_end;               /* first byte past subtable  */
   INSTRUCTION   fi;                    /* subtable inst struct      */
   int           rc;
   int           opcode       = 0;
   int           current_size;
   int           total_size   = 0;
   char        * mod_rm;                /* R/M byte                  */
   int           ocmask       = 0;      /* 2nd or 3rd opcode byte    */
   BOOL          found        = FALSE;  /* subtable instr flag       */
   int           ox;                    /* operand index             */

   MVMsg(gc_mask_Show_Disasm,("\n => i386_instruction( %p, %p, %d, %p, %d )\n",
              data, (int)addr, size, buf, mode ));

   *buf      = 0;                       /* Initialize */
   ooride    = 0;
   preStr[0] = '\0';

   prefix.p_valid = 1;
   clear_prefix_values();

   if ( 0 == mode )                     // 16-bit mode
   {
      prefix.p_os = prefix.p_as = TRUE; /* 16 bit code */
   }

   rc = 0;

   while (rc == 0)
   {                                    /* strip prefixes */
      opcode = (OPC_MASK & *data);
      inst   = i386_instruction_tbl + opcode;

      if (inst->type & PREFIX
          || ( SDFLAG_MODULE_64BIT == mode && ( inst->type & REXPRE ) ) )
      {
         MVMsg(gc_mask_Show_Disasm,(" Prefix: %02x\n", *data));

         set_prefix_value(*data);
         data++;
         total_size++;
      }
      else rc = 1;
   }

   mod_rm = data + 1;

   /* table-lookup for 2-byte opcodes beginning with 0x0F */

   if (opcode == 0x0f)
   {
      mod_rm = data + 2;

      opcode = (OPC_MASK & *(data + 1));
      inst = i386_instruction_tbl_2b + opcode;
   }

   fi = *inst;

   while (inst->type & SUBTB)
   {
      found = FALSE;
      MVMsg(gc_mask_Show_Disasm,(" -- SUBTB opc: %02x\n", data[0]));

      ocmask  = inst->mask;
      tbl_end = (INSTRUCTION *)(inst->mnemonic) + inst->length;
      inst    = (INSTRUCTION *)(inst->mnemonic);

      for (; inst < tbl_end; inst++)
      {
         if ((inst->type & OPC_MASK) == (*mod_rm & ocmask))
         {
            fi.type     = inst->type;
            fi.mnemonic = inst->mnemonic;
            fi.mask     = inst->mask;
            fi.length   = inst->length;

            for (ox = 0; ox < MAX_OPERANDS; ox++)
            {
               /* assumes fi's int's are initialized to 0 */
               /* if not zero then fill from inst         */
               /* this is how or-ing takes place          */
               if (!fi.par[ox].meth) fi.par[ox].meth = inst->par[ox].meth;
               if (!fi.par[ox].type) fi.par[ox].type = inst->par[ox].type;
            }
            found = TRUE;
            break;
         }
      }

      if (!found)
      {
         OptVMsg(" INVALID_2 : Instruction Not Found @ %016" I64LL "X\n", (uint64)addr );   //STJ64
         OptVMsg("   istop %d : %02x %02x %02x\n",
                 istop, (*data), (*(data + 1)), (*(data + 2)) );

         if (istop == 1)
         {
            panic(" INVALID_INSTRUCTION_2 : SubTbl Logic");
         }

         current_size = 2;
         strcpy(OPNM,"INVALID");

         return(OPNM);
      }

      if (opcode == 0x0f) mod_rm = data + 2;
   }

   current_size = decode_instruction(data, mod_rm, &fi, addr, buf);

   if (gc_mask_Show_Disasm & gc.mask)
   {
      show_inst(&fi);
   }

   data       += current_size;
   total_size += current_size;
   *size       = total_size;

   if (ooride == 1)
   {
      strcat(buf, preStr);
   }

   fflush(stdout);
   return(OPNM);
}

#endif


/****************************/
int noStopOnInvalid(void)
{
   //fprintf(stderr, " noStopOnInvalid\n");
   fflush(stderr);
   istop = 0;                           /* def: stop (istop = 1) */
   return(0);
}


/************************/
int inst_type(char * opc)
{
   int oc;

   if ( strncmp(opc, "call", 4) == 0 || strcmp(opc, "syscall") == 0 )
   {
      oc = 1;
   }
   else if ( strncmp(opc, "ret" , 3) == 0 || strcmp(opc, "sysret" ) == 0 )
   {
      oc = 2;
   }
   else if (strncmp(opc, "j"  ,  1) == 0)
   {
      oc = 3;
   }
   else if (strncmp(opc, "iret", 4) == 0)
   {
      oc = 4;
   }
   else
   {
      oc = 5;
   }
   return(oc);
}
