/** $RCSfile: opcode.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: opcode.h,v 1.4 2003/03/21 13:12:03 gautran Exp $ 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
	/************************************************/
	/*	 	opcode.h			*/
	/************************************************/
#ifndef __OPCODE_H_
#define __OPCODE_H_

#define caseop(X)	case X


enum { 	MAX_NAMES 	 = 40000,
	MAX_LABELS 	 = 40000,
	MAX_CODE	 = 80000,
	MAX_HASH_ENTRIES = 2000,
	MAX_PROC_ENTRIES = 2000,
	MAX_INSTRUCTIONS = 135,
	ASCII_SIZE       = 128,
	OP_HASH_SIZE	 = 143,
	BT_HASH_SIZE	 = 197,
	NM_HASH_SIZE	 = 4157,
	LB_HASH_SIZE	 = 4157,
	MAX_NODES	 = 8000,
	MAX_LEN		 = 80,
	MAX_VAR		 = 8192,
	MAX_ARITY	 = 511,
	};

	/************************************************/
	/*	 	Load type                       */
	/* 	0: a direct number (op, int, var_index)	*/
	/*	1: a constant order                     */
	/*	2: a label (offset, tag)                */
	/*	3: a functor (fun order, arity)         */
	/*	4: a builtin index                      */
	/*	5: a hash table                         */
	/************************************************/

#define NUM_TYPE	(0)
#define LAB_TYPE	(1)


 	/************************************************/
	/*	 	builtin table                       */
	/************************************************/

enum InstType {	I_STATIONARY,
		I_WORKER,
		I_MESSENGER,
		I_O,		// no argument
		I_N,		// number
		I_E,		// label
		I_C,		// constant (char string)
		I_F,		// functor
		I_B,		// builtin
		I_I,		// integer
		I_P,		// procedure
		I_R,		// float
 		I_2N,
		I_3N,
		I_4N,
		I_5N,
		I_6N,
		I_7N,
		I_8N,
		I_NE,
		I_NC,
		I_NF,		// integer functor
		I_NI,
		I_NR,
 		I_2NE,
		I_2NF,
		I_NFE,
 		I_2NFE,
		I_3NE,
		I_3NFE,
		I_4E,
		I_N4E,
 		I_HTAB,		// hashtable
 	      };



typedef struct OpCodeRec {
	char* name;
	int opcode;
	int type;
} OPCODEREC;


typedef struct ProcRec {
	int term;
	int loc;
} PROCREC;


extern OPCODEREC opcode_tab[];

	/************************************************/
	/*	 	Integer Constants               */
	/************************************************/

extern int	con_nil;
extern int	con_float;
extern int	con_list;

#define MLVM_ZERO	(0x00000003)
#define MLVM_NONE	(0xfffffffb)
#define MLVM_ONE	(0x0000000b)
#define	MLVM_NOTAG_ONE	(0x00000008)
#define MLVM_FIRST	(0)
#define MLVM_NEXT	(1)

	/************************************************/
	/*	 	Tags                            */
	/* 	00: REF (self-referencial unbound)      */
	/*	01: LIS                                 */
	/*	10: STR	FLP                             */
	/*	11: CON INT FVL FUN                     */
	/************************************************/

#define REF_TAG 	(0x00000000)
#define LIS_TAG 	(0x00000001)
#define STR_TAG 	(0x00000002)
#define CST_TAG 	(0x00000003)

	/************************************************/
	/*	 	Sub-Tags                        */
	/* 	011: INT (29-bit int arithematic)       */
	/*	111: CON (28-bit index)                 */
 	/************************************************/

#define INT_TAG 	(0x00000003)
#define CON_TAG 	(0x00000007)
#define FVL_TAG 	(0xffffffff)

	/************************************************/
	/*	 	Mask/shift Constants            */
	/************************************************/

#define TG2_MASK 	(0x00000003)
#define TG3_MASK 	(0x00000007)
#define ADR_MASK 	(0xfffffffc)
#define FUN_MASK 	(0x007fffff)

#define TG2_SHIFT	(2)
#define TG3_SHIFT	(3)
#define ARITY_SHIFT	(23)


 	/************************************************/
	/*	 	Macros                          */
	/************************************************/


#define addr(T)		(int*)((unsigned int)(T) & ADR_MASK)

#define tag(T)		((int)(T) & TG2_MASK)
#define tag3(T)		((int)(T) & TG3_MASK)

#define is_cst(T)	(tag(T) == CST_TAG)
#define is_con(T)	(tag3(T) == CON_TAG)
#define is_int(T)	(tag3(T) == INT_TAG)
#define is_nil(T)	((int)(T) == con_nil)
#define is_list(T)	(tag(T) == LIS_TAG)
#define is_str(T)	(tag(T) == STR_TAG)
#define is_var(T)	(tag(T) == REF_TAG)
#define is_float(T)	((tag(T) == STR_TAG) && (*addr(T) ==  FVL_TAG))
#define is_fvl(T)	((T) == FVL_TAG)


// is_self_ref and not_self_ref must be used under the condition that
// the term being checked is_var

#define is_self_ref(T)	(*(int*)(T) == (int)(T))
#define not_self_ref(T) (*(int*)(T) != (int)(T))

#define cst_tag(T)	((T) == CST_TAG)
#define con_tag(T)	((T) == CON_TAG)
#define int_tag(T)	((T) == INT_TAG)
#define list_tag(T)	((T) == LIS_TAG)
#define str_tag(T)	((T) == STR_TAG)
#define var_tag(T)	((T) == REF_TAG)

#define make_list(T)	((int)(T) | LIS_TAG)
#define make_str(T)	((int)(T) | STR_TAG)
#define make_flp(T)	((int)(T) | STR_TAG)
#define make_ref(T)	(int)(T)

#define make_int(X)	(((X) << TG3_SHIFT) | INT_TAG)
#define make_fun(F, A)	(((A) << ARITY_SHIFT) | ((F) << TG3_SHIFT) | CON_TAG)
#define make_con(X)	(((X) << TG3_SHIFT) | CON_TAG)

extern int make_float(double, int*);

#define int_value(X)	((X) >> TG3_SHIFT)
#define con_value(X)	((X) >> TG3_SHIFT)
#define fun_value(X)	(((X) & FUN_MASK) >> TG3_SHIFT)
#define lab_value(X)	((X) >> TG2_SHIFT)
#define arity(X)	((unsigned int)(X) >> ARITY_SHIFT)

extern double float_value(int);


/* Control Instructions	*/

#define LVM_NOOP              (0)
#define LVM_ALLOA             (1)
#define LVM_ALLOD             (2)
#define LVM_CALL              (3)
#define LVM_LASTCALL          (4)
#define LVM_CHAINCALL         (5)
#define LVM_PROCEED           (6)
#define LVM_RETURN            (7)
#define LVM_SHALLOWTRY        (8)
#define LVM_TRY               (9)
#define LVM_RETRY             (10)
#define LVM_TRUST             (11)
#define LVM_NECKCUT           (12)
#define LVM_DEEPLABEL         (13)
#define LVM_DEEPCUT           (14)
#define LVM_BUILTIN           (15)
#define LVM_FAIL              (16)
#define LVM_FINISH            (17)

/* Unification Instructions	*/

/* PUT set (parameter setting)	*/

#define LVM_PUTVOID           (18)
#define LVM_PUT2VOID          (19)
#define LVM_PUTVAR            (20)
#define LVM_PUT2VAR           (21)
#define LVM_PUT3VAR           (22)
#define LVM_PUT4VAR           (23)
#define LVM_PUTRVAL           (24)
#define LVM_PUTVAL            (25)
#define LVM_PUT2VAL           (26)
#define LVM_PUT3VAL           (27)
#define LVM_PUT4VAL           (28)
#define LVM_PUT5VAL           (29)
#define LVM_PUT6VAL           (30)
#define LVM_PUT7VAL           (31)
#define LVM_PUT8VAL           (32)
#define LVM_PUTCON            (33)
#define LVM_PUTINT            (34)
#define LVM_PUTLIST           (35)
#define LVM_PUTSTR            (36)
#define LVM_RESV0             (37)
#define LVM_RESV1             (38)

/* SET set (write stream)	*/

#define LVM_SETVAR            (39)
#define LVM_CSETVAR           (40)
#define LVM_SETCON            (41)
#define LVM_CSETCON           (42)
#define LVM_SETINT	      (43)
#define LVM_CSETINT	      (44)
#define LVM_SETVAL            (45)
#define LVM_CSETVAL           (46)
#define LVM_SETFLOAT          (47)
#define LVM_CSETFLOAT         (48)
#define LVM_SETLIST           (49)
#define LVM_CSETLIST          (50)
#define LVM_SETFUN            (51)
#define LVM_CSETFUN           (52)
#define LVM_SETSTR            (53)
#define LVM_CSETSTR           (54)
#define LVM_RESV2             (55)
#define LVM_RESV3             (56)

/* Stream jump			*/

#define LVM_JUMP              (57)
#define LVM_BRANCH            (58)

/* GET set (read stream)	*/

#define LVM_GETCON            (59)
#define LVM_CGETCON           (60)
#define LVM_GETINT            (61)
#define LVM_CGETINT           (62)
#define LVM_GETVAL            (63)
#define LVM_CGETVAL           (64)
#define LVM_GETFLOAT          (65)
#define LVM_CGETFLOAT         (66)
#define LVM_GETLIST           (67)
#define LVM_CGETLIST          (68)
#define LVM_GETVLIST          (69)
#define LVM_CGETVLIST         (70)
#define LVM_GETSTR            (71)
#define LVM_CGETSTR           (72)

/* Arithmetic Instruction	*/

#define LVM_ADD               (73)
#define LVM_SUB               (74)
#define LVM_MUL               (75)
#define LVM_DIV               (76)
#define LVM_MINUS             (77)
#define LVM_MOD               (78)
#define LVM_INC               (79)
#define LVM_DEC               (80)
#define LVM_IS                (81)
#define LVM_AND               (82)
#define LVM_OR                (83)
#define LVM_EOR               (84)
#define LVM_RSHIFT            (85)
#define LVM_LSHIFT            (86)

/* Meta-predicates	*/

#define LVM_ISATOM            (87)
#define LVM_ISATOMIC          (88)
#define LVM_ISFLOAT           (89)
#define LVM_ISINTEGER         (90)
#define LVM_ISNUMBER          (91)
#define LVM_ISNONVAR          (92)
#define LVM_ISVAR             (93)
#define LVM_ISLIST            (94)
#define LVM_ISSTR             (95)
#define LVM_ISCOMPOUND        (96)
#define LVM_ISGROUND          (97)
#define LVM_ISNIL             (98)
#define LVM_LASTNIL           (99)
#define LVM_ISZERO            (100)
#define LVM_LASTZERO          (101)

/* Dispatching Instructions	*/

#define LVM_SWITCH            (102)
#define LVM_LASTSWITCH        (103)
#define LVM_HASHING           (104)
#define LVM_LASTHASHING       (105)

/* Term Comparison Instructions */

#define LVM_IDENTICAL         (106)
#define LVM_NONIDENTICAL      (107)
#define LVM_PRECEDE           (108)
#define LVM_PREIDENTICAL      (109)
#define LVM_FOLLOW            (110)
#define LVM_FLOIDENTICAL      (111)
#define LVM_TERMCOMPARE       (112)

/* Arithmetic Comparison Instructions */

#define LVM_IFEQ              (113)
#define LVM_IFNE              (114)
#define LVM_IFLT              (115)
#define LVM_IFLE              (116)
#define LVM_IFGT              (117)
#define LVM_IFGE              (118)
#define LVM_ARICMP            (119)
#define LVM_IFEQ0             (120)
#define LVM_IFNE0             (121)
#define LVM_IFLT0             (122)
#define LVM_IFLE0             (123)
#define LVM_IFGT0             (124)
#define LVM_IFGE0             (125)

/* Unification Instructions */

#define LVM_UNIFIABLE         (126)
#define LVM_NONUNIFIABLE      (127)
#define LVM_TRYUNIFIABLE      (128)

/* Type Casting Instructions	*/

#define LVM_INTEGER           (129)
#define LVM_CHAR              (130)
#define LVM_FLOAT             (131)
 
/* Imago Instructions	*/

#define LVM_STATIONARY        (132)
#define LVM_WORKER            (133)
#define LVM_MESSENGER         (134)
 
 /* Directives	*/

#define LVM_PROCEDURE         (135)
#define LVM_HASHTABLE         (136)
#endif
