/** $RCSfile: opcode.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: opcode.c,v 1.3 2003/03/21 13:12:03 gautran Exp $ 
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
/*              opcode.c                       */
/************************************************/

#include <stdio.h>
#include "opcode.h"

int con_nil;
int con_float = FVL_TAG;
int con_list;

OPCODEREC opcode_tab[] = {
    {"noop", LVM_NOOP, I_O},
    {"alloa", LVM_ALLOA, I_2N},
    {"allod", LVM_ALLOD, I_2N},
    {"call", LVM_CALL, I_E},
    {"lastcall", LVM_LASTCALL, I_E},
    {"chaincall", LVM_CHAINCALL, I_E},
    {"proceed", LVM_PROCEED, I_O},
    {"return", LVM_RETURN, I_O},
    {"shallowtry", LVM_SHALLOWTRY, I_NE},
    {"try", LVM_TRY, I_NE},
    {"retry", LVM_RETRY, I_NE},
    {"trust", LVM_TRUST, I_O},
    {"neckcut", LVM_NECKCUT, I_O},
    {"deeplabel", LVM_DEEPLABEL, I_N},
    {"deepcut", LVM_DEEPCUT, I_N},
    {"builtin", LVM_BUILTIN, I_B},
    {"fail", LVM_FAIL, I_O},
    {"finish", LVM_FINISH, I_O},

    {"putvoid", LVM_PUTVOID, I_O},
    {"put2void", LVM_PUT2VOID, I_O},
    {"putvar", LVM_PUTVAR, I_N},
    {"put2var", LVM_PUT2VAR, I_2N},
    {"put3var", LVM_PUT3VAR, I_3N},
    {"put4var", LVM_PUT4VAR, I_4N},
    {"putrval", LVM_PUTRVAL, I_N},
    {"putval", LVM_PUTVAL, I_N},
    {"put2val", LVM_PUT2VAL, I_2N},
    {"put3val", LVM_PUT3VAL, I_3N},
    {"put4val", LVM_PUT4VAL, I_4N},
    {"put5val", LVM_PUT5VAL, I_5N},
    {"put6val", LVM_PUT6VAL, I_6N},
    {"put7val", LVM_PUT7VAL, I_7N},
    {"put8val", LVM_PUT8VAL, I_8N},
    {"putcon", LVM_PUTCON, I_C},
    {"putint", LVM_PUTINT, I_I},
    {"putlist", LVM_PUTLIST, I_N},
    {"putstr", LVM_PUTSTR, I_N},
    {"resv0", LVM_RESV0, I_N},
    {"resv1", LVM_RESV1, I_N},

    {"setvar", LVM_SETVAR, I_N},
    {"csetvar", LVM_CSETVAR, I_O},
    {"setcon", LVM_SETCON, I_NC},
    {"csetcon", LVM_CSETCON, I_C},
    {"setint", LVM_SETINT, I_NI},
    {"csetint", LVM_CSETINT, I_I},
    {"setval", LVM_SETVAL, I_2N},
    {"csetval", LVM_CSETVAL, I_N},
    {"setfloat", LVM_SETFLOAT, I_NR},
    {"csetfloat", LVM_CSETFLOAT, I_R},
    {"setlist", LVM_SETLIST, I_2N},
    {"csetlist", LVM_CSETLIST, I_N},
    {"setfun", LVM_SETFUN, I_NF},
    {"csetfun", LVM_CSETFUN, I_F},
    {"setstr", LVM_SETSTR, I_2N},
    {"csetstr", LVM_CSETSTR, I_N},
    {"resv2", LVM_RESV2, I_N},
    {"resv3", LVM_RESV3, I_N},

    {"jump", LVM_JUMP, I_E},
    {"branch", LVM_BRANCH, I_4E},

    {"getcon", LVM_GETCON, I_NC},
    {"cgetcon", LVM_CGETCON, I_C},
    {"getint", LVM_GETINT, I_NI},
    {"cgetint", LVM_CGETINT, I_I},
    {"getval", LVM_GETVAL, I_2N},
    {"cgetval", LVM_CGETVAL, I_N},
    {"getfloat", LVM_GETFLOAT, I_NR},
    {"cgetfloat", LVM_CGETFLOAT, I_R},
    {"getlist", LVM_GETLIST, I_3NE},
    {"cgetlist", LVM_CGETLIST, I_2NE},
    {"getvlist", LVM_GETVLIST, I_2N},
    {"cgetvlist", LVM_CGETVLIST, I_N},
    {"getstr", LVM_GETSTR, I_3NFE},
    {"cgetstr", LVM_CGETSTR, I_2NFE},
  
    {"add", LVM_ADD, I_3N},
    {"sub", LVM_SUB, I_3N},
    {"mul", LVM_MUL, I_3N},
    {"div", LVM_DIV, I_3N},
    {"minus", LVM_MINUS, I_2N},
    {"mod", LVM_MOD, I_3N},
    {"inc", LVM_INC, I_2N},
    {"dec", LVM_DEC, I_2N},
    {"is", LVM_IS, I_2N},
    {"and", LVM_AND, I_3N},
    {"or", LVM_OR, I_3N},
    {"eor", LVM_EOR, I_3N},
    {"rshift", LVM_RSHIFT, I_3N},
    {"lshift", LVM_LSHIFT, I_3N},

    {"isatom", LVM_ISATOM, I_NE},
    {"isatomic", LVM_ISATOMIC, I_NE},
    {"isfloat", LVM_ISFLOAT, I_NE},
    {"isinteger", LVM_ISINTEGER, I_NE},
    {"isnumber", LVM_ISNUMBER, I_NE},
    {"isnonvar", LVM_ISNONVAR, I_NE},
    {"isvar", LVM_ISVAR, I_NE},
    {"islist", LVM_ISLIST, I_NE},
    {"isstr", LVM_ISSTR, I_NE},
    {"iscompound", LVM_ISCOMPOUND, I_NE},
    {"isground", LVM_ISGROUND, I_NE},
    {"isnil", LVM_ISNIL, I_NE},
    {"lastnil", LVM_LASTNIL, I_E},
    {"iszero", LVM_ISZERO, I_NE},
    {"lastzero", LVM_LASTZERO, I_E},

    {"switch", LVM_SWITCH, I_N4E},
    {"lastswitch", LVM_LASTSWITCH, I_4E},
    {"hashing", LVM_HASHING, I_NE},
    {"lasthashing", LVM_LASTHASHING, I_E},

    {"identical", LVM_IDENTICAL, I_2NE},
    {"nonidentical", LVM_NONIDENTICAL, I_2NE},
    {"precede", LVM_PRECEDE, I_2NE},
    {"preidentical", LVM_PREIDENTICAL, I_2NE},
    {"follow", LVM_FOLLOW, I_2NE},
    {"floidentical", LVM_FLOIDENTICAL, I_2NE},
    {"termcompare", LVM_TERMCOMPARE, I_3N},

    {"ifeq", LVM_IFEQ, I_2NE},
    {"ifne", LVM_IFNE, I_2NE},
    {"iflt", LVM_IFLT, I_2NE},
    {"ifle", LVM_IFLE, I_2NE},
    {"ifgt", LVM_IFGT, I_2NE},
    {"ifge", LVM_IFGE, I_2NE},
    {"aricmp", LVM_ARICMP, I_3N},
    {"ifeq0", LVM_IFEQ0, I_NE},
    {"ifne0", LVM_IFNE0, I_NE},
    {"iflt0", LVM_IFLT0, I_NE},
    {"ifle0", LVM_IFLE0, I_NE},
    {"ifgt0", LVM_IFGT0, I_NE},
    {"ifge0", LVM_IFGE0, I_NE},

    {"unifiable", LVM_UNIFIABLE, I_2NE},
    {"nonunifiable", LVM_NONUNIFIABLE, I_2NE},
    {"tryunifiable", LVM_TRYUNIFIABLE, I_3N},

    {"integer", LVM_INTEGER, I_N},
    {"char", LVM_CHAR, I_N},
    {"float", LVM_FLOAT, I_N},

    {"stationary", LVM_STATIONARY, I_STATIONARY},
    {"worker", LVM_WORKER, I_WORKER},
    {"messenger", LVM_MESSENGER, I_MESSENGER},
  
    {"procedure", LVM_PROCEDURE, I_P},
    {"hashtable", LVM_HASHTABLE, I_HTAB},
    { (char*) 0, 0,  0}
};



double float_value(int fterm){ // tag has been checked
    union {
	double f;
	int v[2];
    } v_f;
    int *fp = addr(fterm);
    v_f.v[0] = *(fp + 1); // W1
    v_f.v[1] = *(fp + 2); // W2
    return v_f.f;
}
	
int make_float(double f, int* st){ 
    union {
	double f;
	int v[2];
    } v_f;
    int tmp = make_str(++st);
    *(st) = FVL_TAG;	// special tag
    v_f.f = f;
    *(st + 1) = v_f.v[0]; // W1
    *(st + 2) = v_f.v[1]; // W2
    return tmp;
}
