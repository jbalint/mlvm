/** $RCSfile: builtins.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: builtins.h,v 1.9 2003/03/21 13:13:07 gautran Exp $ 
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

#ifndef __BUILTINS_H_
#define __BUILTINS_H_
/****************************************************************/
/*			Macros (BUILTINS)			*/
/****************************************************************/

#define bt_deref(X)	while(is_var(X) && (X) != *(int*)(X)) X = *(int*)(X)
#define bt_trail(X, IP) if ((int*)(X) <= (IP)->gl) { *(IP)->tt-- = (int*)(X);}
#define bt_bbtrail(X, IP) if ((int*)(X) <= (IP)->bb) { *(IP)->tt-- = (int*)(X);}

#define LEGAL_ALL         0xff
#define LEGAL_NONE        0x00
#define LEGAL_STATIONARY  0x01
#define LEGAL_WORKER      0x02
#define LEGAL_MESSENGER   0x04
#define LEGAL_SYSMSGER    0x08
#define LEGAL_SANDW       (LEGAL_STATIONARY | LEGAL_WORKER)
#define LEGAL_WANDM       (LEGAL_WORKER | LEGAL_MESSENGER)


#ifdef __IMAGO_ICB_H_
# define BP_DECLARATION(X) void bp_##X(ICBPtr)
typedef void (*Builtin)(void*);
#else
# ifdef __BUILTINS_C_
#  define BP_DECLARATION(X) inline void bp_##X() {}
# else
#  define BP_DECLARATION(X) inline void bp_##X()
# endif
typedef void (*Builtin)();
#endif

typedef struct FuncRec {
    char* name;
    char *str;
    int arity;
    Builtin bf;
    int special;
    int legal;
} FUNCREC, SPNODE;

extern FUNCREC btin_tab[];


/** Builtins function declarations (please, keep the alphabetical order). */
BP_DECLARATION(abs);
BP_DECLARATION(accept);
BP_DECLARATION(arccos);
BP_DECLARATION(arcsin);
BP_DECLARATION(arctan);
BP_DECLARATION(arg);
BP_DECLARATION(atom);
BP_DECLARATION(atom_chars);
BP_DECLARATION(atom_concat);
BP_DECLARATION(atom_length);
BP_DECLARATION(atomic);
BP_DECLARATION(attach);
BP_DECLARATION(ceiling);
BP_DECLARATION(char_code);
BP_DECLARATION(clone);
BP_DECLARATION(close1);
BP_DECLARATION(close2);
BP_DECLARATION(compound);
BP_DECLARATION(cos);
BP_DECLARATION(create);
BP_DECLARATION(ctime);
BP_DECLARATION(current_host);
BP_DECLARATION(current_input);
BP_DECLARATION(current_output);
BP_DECLARATION(database_search);
BP_DECLARATION(debugging);
BP_DECLARATION(dispatch);
BP_DECLARATION(dispose);
BP_DECLARATION(float);
BP_DECLARATION(floor);
BP_DECLARATION(flush_output);
BP_DECLARATION(flush_output1);
BP_DECLARATION(functor3);
BP_DECLARATION(get_byte);
BP_DECLARATION(get_byte2);
BP_DECLARATION(get_char);
BP_DECLARATION(get_char2);
BP_DECLARATION(get_code);
BP_DECLARATION(get_code2);
BP_DECLARATION(ground);
BP_DECLARATION(integer);
BP_DECLARATION(integer_float);
BP_DECLARATION(length_2);
BP_DECLARATION(log);
BP_DECLARATION(minus);
BP_DECLARATION(move);
BP_DECLARATION(nl);
BP_DECLARATION(nonvar);
BP_DECLARATION(nospy);
BP_DECLARATION(nospy2);
BP_DECLARATION(notrace);
BP_DECLARATION(number);
BP_DECLARATION(number_chars);
BP_DECLARATION(open3);
BP_DECLARATION(open4);
BP_DECLARATION(peek_byte);
BP_DECLARATION(peek_byte2);
BP_DECLARATION(peek_char);
BP_DECLARATION(peek_char2);
BP_DECLARATION(peek_code);
BP_DECLARATION(peek_code2);
BP_DECLARATION(pow);
BP_DECLARATION(put_byte);
BP_DECLARATION(put_byte2);
BP_DECLARATION(put_char);
BP_DECLARATION(put_char2);
BP_DECLARATION(put_code);
BP_DECLARATION(put_code2);
BP_DECLARATION(rand);
BP_DECLARATION(reset_sender);
BP_DECLARATION(round);
BP_DECLARATION(sender);
BP_DECLARATION(set_input);
BP_DECLARATION(set_output);
BP_DECLARATION(set_stream_position);
BP_DECLARATION(sin);
BP_DECLARATION(sleep);
BP_DECLARATION(spy);
BP_DECLARATION(spy2);
BP_DECLARATION(sqrt);
BP_DECLARATION(srand);
BP_DECLARATION(stationary_host);
BP_DECLARATION(stats);
BP_DECLARATION(sub_atom);
BP_DECLARATION(syscall);
BP_DECLARATION(tan);
BP_DECLARATION(terminate);
BP_DECLARATION(trace);
BP_DECLARATION(univ);
BP_DECLARATION(var);
BP_DECLARATION(wait_accept);
BP_DECLARATION(web_search);
BP_DECLARATION(workers);
BP_DECLARATION(write);
BP_DECLARATION(write2);

#endif
