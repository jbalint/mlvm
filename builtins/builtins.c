/** $RCSfile: builtins.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: builtins.c,v 1.26 2003/03/21 13:12:02 gautran Exp $ 
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
/*	 	builtins.c                      */
/************************************************/
#define __BUILTINS_C_
#include <stdio.h>
#include "opcode.h"
#include "builtins.h"

#define BP_TABLE(X,A,F,L) {  #X"/"#A, #X, A, (Builtin)F, 0, L }


FUNCREC btin_tab[] = {

    /* I think that it is time to finalize our built-in list, I make the 
       following proposal. Some of them have been implemented, and some
       to be finished. We should try to make this list matching the standard
       Prolog. I divide the list into four groups:

       mata-logic	// LEGAL_ALL
       arithmetic and type conversion // LEGAL_ALL
       I/O		// LEGAL_STATIONARY
       imago		// different legalibility

       I use the standard mode to mark each argument:

       +: the argument shall be instantiated
       ?: the argument shall be instantiated or a variable
       -: the argument shall be a variable
       @: the argument shall remain unchanged
    */

    /* (A) meta-logic predicates		*/

    // var(@T): true iff T is a var	
    BP_TABLE( var,                 1, bp_var, LEGAL_ALL ),

    // nonvar(@T): true iff T is not a var	
    BP_TABLE( nonvar,              1, bp_nonvar, LEGAL_ALL),

    // atom(@T): true iff T is an atom 
    BP_TABLE( atom,                1, bp_atom, LEGAL_ALL),

    // integer(@T): true iff T is an integer
    BP_TABLE( integer,             1, bp_integer, LEGAL_ALL),

    // float(@T): true iff T is a float
    BP_TABLE( float,               1, bp_float, LEGAL_ALL),
	
    // atomic(@T): true iff T is a float, or an integer or an atom
    BP_TABLE( atomic,              1, bp_atomic, LEGAL_ALL),
	
    // compound(@T): true iff T is  a list or a structure
    BP_TABLE( compound,            1, bp_compound, LEGAL_ALL),

    // ground(@T): true iff T is completely instantiated (with no vars) 
    BP_TABLE( ground,              1, bp_ground, LEGAL_ALL),

    // number(@T): true iff T is a float or an integer
    BP_TABLE( number,              1, bp_number, LEGAL_ALL),

    /* (B)	arithmetic and term conversion and decomposition // LEGAL_ALL */

    // abs(+number, -number) : convert to abs value
    BP_TABLE( abs,                 2, bp_abs, LEGAL_ALL),

    // ceiling(+number, -integer): return the smallest integer >= +number
    BP_TABLE( ceiling,             2, bp_ceiling, LEGAL_ALL),


    // floor(+number, -integer) : return the largest integer <= +number
    BP_TABLE( floor,               2, bp_floor, LEGAL_ALL),

    // sqrt(+number -number): return the sqrt 
    BP_TABLE( sqrt,                2, bp_sqrt, LEGAL_ALL),

    // the followings are straightforward
    BP_TABLE( log,                 2, bp_log, LEGAL_ALL),
    BP_TABLE( minus,               2, bp_minus, LEGAL_ALL),
    BP_TABLE( pow,                 2, bp_pow, LEGAL_ALL),
    BP_TABLE( round,               2, bp_round, LEGAL_ALL),
    BP_TABLE( sin,                 2, bp_sin, LEGAL_ALL),
    BP_TABLE( arcsin,              2, bp_arcsin, LEGAL_ALL),
    BP_TABLE( cos,                 2, bp_cos, LEGAL_ALL),
    BP_TABLE( arccos,              2, bp_arccos, LEGAL_ALL),
    BP_TABLE( tan,                 2, bp_tan, LEGAL_ALL),
    BP_TABLE( arctan,              2, bp_arctan, LEGAL_ALL),

    // rand(-float) : return a uniform random number in 0-1
    BP_TABLE( rand,                1, bp_rand, LEGAL_ALL),

    // srand(+number, -float): use +number as seed and as above
    BP_TABLE( srand,               2, bp_srand, LEGAL_ALL),

    // functor(-nonvar, +atomic, +integer) or functor(+nonvar, ?atomic, +integer)
    // composing/decomposing a structure	
    BP_TABLE( functor,             3, bp_functor3, LEGAL_ALL),

    // arg(+integer, +compound, ?term) : accessing argument
    BP_TABLE( arg,                 3, bp_arg, LEGAL_ALL),

    // length(+list, ?integer): get the length of a list
    BP_TABLE( length,              2, bp_length_2, LEGAL_ALL),

    // univ(?term, ?term): unify terms
    BP_TABLE( univ,                2, bp_univ, LEGAL_ALL),

    // atom_length(+atom, ?length): get the length of an atom
    BP_TABLE( atom_length,         2, bp_atom_length, LEGAL_ALL),

    // atom_concat(+atom, +atom, -atom): cancat two atoms into one
    // ** we only implement this at this moment, in std Prolog, there is another mode
    // atom_cancat(?atom, ?atom, +atom) will is backtrackable
    BP_TABLE( atom_concat,         3, bp_atom_concat, LEGAL_ALL),

    // sub_atom(+atom, ?integer, ?integer, ?integer, ?atom): extract a sub-atom
    // example: sub_atom(abcdefg, 5, _, T) -> T will be 'abcde'
    //	    sub_atom(abcdefg, _, 0, T) -> T will be 'efg'
    //	    sub_atom(abcdefg, 4, _, T) -> T will be 'cdef'
    BP_TABLE( sub_atom,            4, bp_sub_atom, LEGAL_ALL),

    // atom_chars(+atom, ?list) or atom_chars(-atom, +list)
    // convert an atom to a list of chars, or reverse
    BP_TABLE( atom_chars,          2, bp_atom_chars, LEGAL_ALL),

    // char_code(+char, ?integer) or char_code(-char, +integer)
    // convert a char to an integer (ASCII code) or reverse
    BP_TABLE( char_code,           2, bp_char_code, LEGAL_ALL),

    // number_chars(+integer, ?list) or number_chars(-atom, +list)
    // convert a number to a list of chars, or reverse
    BP_TABLE( number_chars,        2, bp_number_chars, LEGAL_ALL),

    // integer_float(+integer, ?float) or char_code(-integer, +float)
    // convert a char to an integer (ASCII code) or reverse
    BP_TABLE( integer_float,       2, bp_integer_float, LEGAL_ALL),

    /* (C) I/O, used only by stationary */
    /* Std Prolog presents a set of I/O different from tranditional I/O
       and here I would like to follow the Std Prolog 
       there are 3 i/o modes:
       read, write, and append
       and there are two stream options type(T) where T shall be
       text or binary (the default could be text)

       We might not to implement all I/O predicates bu have to provide the
       basic ones.
    */

    // current_input(?stream) : true iff ?stream matches the current input
    BP_TABLE( current_input,       1, bp_current_input, LEGAL_STATIONARY),

    // current_output(?stream) : true iff ?stream matches the current output
    BP_TABLE( current_output,      1, bp_current_output, LEGAL_STATIONARY),

    // set_input(@stream): set the current input to @stream
    BP_TABLE( set_input,           1, bp_set_input, LEGAL_STATIONARY),
 
    // set_output(@stream): set the current output to @stream 
    BP_TABLE( set_output,          1, bp_set_output, LEGAL_STATIONARY),

    // open(@file, @mode, -stream, @option_list): open for i/o
    // example: open("/xli/bench/qsort", read, R, [type(binary)])
    //          open("/xli/data", write, W, [])
    BP_TABLE( open,                4, bp_open4, LEGAL_STATIONARY),

    // open(@file, @mode, -stream): open for i/o
    // example: open("/xli/bench/qsort", read, R)
    //          open("/xli/data", write, W)
    BP_TABLE( open,                3, bp_open3, LEGAL_STATIONARY),

    // close(@stream, @close_option): close the @stream
    BP_TABLE( close,               2, bp_close2, LEGAL_STATIONARY),

    // close(@stream): close the @stream
    BP_TABLE( close,               1, bp_close1, LEGAL_STATIONARY),

    // flush_output(@stream): flush the output
    BP_TABLE( flush_output,        1, bp_flush_output1, LEGAL_STATIONARY),

    // flush_output: flush the current output
    BP_TABLE( flush_output,        0, bp_flush_output, LEGAL_STATIONARY),

    // nl: write a newline to the current output
    BP_TABLE( nl,                  0, bp_nl,            LEGAL_ALL),

    // set_stream_position(@stream, +position): position is impl dependent
    // to be decided
    BP_TABLE( set_stream_position, 2, bp_set_stream_position, LEGAL_STATIONARY),

    // get_char(?char) or get_char(@stream, ?char)
    // get a char (or code) form @stream or the current input
    BP_TABLE( get_char,            1, bp_get_char, LEGAL_STATIONARY),
    BP_TABLE( get_char,            2, bp_get_char2, LEGAL_STATIONARY),
    BP_TABLE( get_code,            1, bp_get_code, LEGAL_STATIONARY),
    BP_TABLE( get_code,            2, bp_get_code2, LEGAL_STATIONARY),

    // peek_char(?char) or peek_char(@stream, ?char)
    // peek a char (or code) form @stream or the current input
    BP_TABLE( peek_char,           1, bp_peek_char, LEGAL_STATIONARY),
    BP_TABLE( peek_char,           2, bp_peek_char2, LEGAL_STATIONARY),
    BP_TABLE( peek_code,           1, bp_peek_code, LEGAL_STATIONARY),
    BP_TABLE( peek_code,           2, bp_peek_code2, LEGAL_STATIONARY),

    // put_char(+char) or put_char(@stream, +char)
    // put a char (or code) to @stream or the current input
    BP_TABLE( put_char,            1, bp_put_char, LEGAL_STATIONARY),
    BP_TABLE( put_char,            2, bp_put_char2, LEGAL_STATIONARY),
    BP_TABLE( put_code,            1, bp_put_code, LEGAL_STATIONARY),
    BP_TABLE( put_code,            2, bp_put_code2, LEGAL_STATIONARY),

    // get_byte(?char) or get_byte(@stream, ?char) for binary i/o
    // get a char (or code) form @stream or the current input
    BP_TABLE( get_byte,            1, bp_get_byte, LEGAL_STATIONARY),
    BP_TABLE( get_byte,            2, bp_get_byte2, LEGAL_STATIONARY),
 
    // peek_byte(?char) or peek_byte(@stream, ?char)
    // peek a char (or code) form @stream or the current input
    BP_TABLE( peek_byte,           1, bp_peek_byte, LEGAL_STATIONARY),
    BP_TABLE( peek_byte,           2, bp_peek_byte2, LEGAL_STATIONARY),
  
    // put_byte(+char) or put_byte(@stream, +char)
    // put a char (or code) to @stream or the current input
    BP_TABLE( put_byte,            1, bp_put_byte, LEGAL_STATIONARY),
    BP_TABLE( put_byte,            2, bp_put_byte2, LEGAL_STATIONARY),
 
    // We do not provide read_term predicates

    // write(@stream, +term) or write(+term)
    // write the +term to the @stream or the current output
    BP_TABLE( write,               1, bp_write, LEGAL_ALL),
    BP_TABLE( write,               2, bp_write2, LEGAL_STATIONARY),


    // I leave this set of predicates here but may not touch them again

    BP_TABLE( spy,                 0, bp_spy, LEGAL_ALL),
    BP_TABLE( nospy,               0, bp_nospy, LEGAL_ALL),
    BP_TABLE( spy,                 2, bp_spy2, LEGAL_ALL),
    BP_TABLE( nospy,               2, bp_nospy2, LEGAL_ALL),
    BP_TABLE( trace,               0, bp_trace, LEGAL_ALL),
    BP_TABLE( notrace,             0, bp_notrace, LEGAL_ALL),
    BP_TABLE( debugging,           0, bp_debugging, LEGAL_ALL),

    /* (D) imago predicates		*/ 

    BP_TABLE( ctime,               1, bp_ctime, LEGAL_ALL),
    BP_TABLE( sleep,               1, bp_sleep, LEGAL_ALL),
    BP_TABLE( stats,               1, bp_stats, LEGAL_ALL),
 
    BP_TABLE( accept,              2, bp_accept, LEGAL_SANDW),
    BP_TABLE( attach,              3, bp_attach, LEGAL_MESSENGER),
    BP_TABLE( clone,               2, bp_clone, LEGAL_WANDM),
    BP_TABLE( create,              3, bp_create, LEGAL_STATIONARY),
    BP_TABLE( current_host,        1, bp_current_host, LEGAL_ALL),
    BP_TABLE( database_search,     3, bp_database_search, LEGAL_WORKER),
    BP_TABLE( dispatch,            3, bp_dispatch, LEGAL_SANDW),
    BP_TABLE( dispose,             0, bp_dispose, LEGAL_WANDM),
    BP_TABLE( move,                1, bp_move, LEGAL_WANDM),
    BP_TABLE( reset_sender,        1, bp_reset_sender, LEGAL_MESSENGER),
    BP_TABLE( sender,              1, bp_sender, LEGAL_MESSENGER),
    BP_TABLE( stationary_host,     1, bp_stationary_host, LEGAL_ALL),
    BP_TABLE( terminate,           0, bp_terminate, LEGAL_STATIONARY),
    BP_TABLE( wait_accept,         2, bp_wait_accept, LEGAL_SANDW),
    BP_TABLE( web_search,          3, bp_web_search, LEGAL_WORKER),
    BP_TABLE( workers,             3, bp_workers, LEGAL_ALL),

    BP_TABLE( syscall,             3, bp_syscall, LEGAL_SYSMSGER),

    /** This is the last entry and should remain as is. */
    { NULL, NULL, 0, (Builtin)NULL, 0, LEGAL_NONE } 
};
