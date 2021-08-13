/** $RCSfile: bp_impl.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: bp_impl.c,v 1.18 2003/03/25 15:34:39 gautran Exp $ 
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
/*	 	bp_impl.c                       */
/************************************************/
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <regex.h>

#include "system.h"
#include "opcode.h"
#include "engine.h"
#include "builtins.c"
/****************************************************************/
/*								*/
/* Builtins are divided into two groups: + and ?. Where the 	*/
/* + group accepts ground arguments only and the ? group might  */
/* bind some variables during execution. A special field in	*/
/* a builtin record indicates its group: 1/0 => +/?.		*/
/* A builtin predicate is invoked by the following instruction:	*/
/*								*/
/* builtin b/k : b/k is the symbolic builtin name with arity	*/
/*								*/
/* During compilation, it is translated to:			*/
/* LW_BUILTIN index  where the index gives the entry            */
/* A single parameter is passed to a builtin function: ICBPtr ip*/
/* through ip->st, builtins can find all arguments it required  */
/* 1) some builtins might carry out binding operation. Bindings */
/* must be trailed in case of GC and backtracking. Thus, we     */
/* provide a set of new macros for this purpose.                */
/* 2) some builtins might involve new term construction on      */
/* the top of stack. If this happens, ip->st must be updated    */
/* 3) I will use T representing trail(), B bbtrail() and        */
/* S stack construct, we mark each builtin if such operations   */
/* are used.                                                    */
/*								*/
/****************************************************************/
int match_messenger(ICBPtr, ICBPtr);
int msg_unify(int*, int*, ICBPtr);
int messenger_accept(ICBPtr);
/****************************************************************/
/**       ! ! ! PLEASE, KEEP THE ALPHA ORDERING ! ! !           */
/****************************************************************/

void bp_accept(ICBPtr ip){
    // accept(Sender, Msg)
    int ret;
    if( (ret = messenger_accept(ip)) == 0 ) {
	/** No messenger received. */
	ip->status = IMAGO_BACKTRACK;
    } else if( ret > 0 ) {
	/** We received a messenger. */
	ip->status = IMAGO_READY;
    } else if( ip->status == IMAGO_EXPANSION ) {
	/** We may need to expand the worker because the messenger carried a huge message. */
    } else {
	/** There was an error. */
	ip->status = IMAGO_ERROR;
    }
 
    return; 
}

void bp_arg(ICBPtr ip){ /* T and B */
    // arg(r1, r2, r3)	=> (+integer, +compound_term, ?term)
 
    register int r3 = *(ip->st);
    register int r2 = *(ip->st - 1);
    register int r1 = *(ip->st - 2);
    register int* r4;

    bt_deref(r1);
    bt_deref(r2);
    bt_deref(r3);

    if (!is_int(r1)){ //  Arg/3: arg1 integer expected
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return; 
    }
    else {
	r1 = int_value(r1);
    }

    if (is_list(r2)){			// a list term
	r4 = addr(r2);			// set r4 pointer to list
	r2 = 2;				// set r2 list-arity
    }
    else if (is_str(r2)){
	r4 = addr(r2);			// r4 points to str's functor
	r2 = arity(*r4);		// set r2 str-arity
	r4++;				// r4 points to str's argument
    }
    else { //  Arg/3: arg2 compound term expected
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return; 
    }

    if (r1 < 1 || r1 > r2){ // Arg/3: arg1 out of range
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return; 
    }

    r4 += r1 - 1;				// r4 points to selected arg

    if (is_var(r3)){
	r1 = *(int*) r3 = *r4;		// assign that arg to r3
	if (is_con(r1)){
	    bt_bbtrail(r3, ip);
	}
	else {
	    bt_trail(r3, ip);
	}
	return;
    }

    while(is_var(r4) && (r4 != (int*)*r4)) r4 = (int*)*r4;

    if (is_var(r4)){			// if arg is an unbound
	*r4 = r3;			// assign r3 to that arg
	if (is_con(r3)){
	    bt_bbtrail(r4, ip);
	}
	else {
	    bt_trail(r4, ip);
	}

	return;
    }
    // if reach here, we need a full unification
    // to be implemented later

    return;

}

void bd_atom_concat(ICBPtr ip){ return; }

void bp_attach(ICBPtr ip){ /* T, B and S */
    // attach(receiver, msg, Result)
    LOGPtr log;
    ICBPtr rip;
    char *name, tname[2], lname[MAX_DNS_NAME_SIZE];
    int t, v, lstatus;
    icb_queue* msq;
    debug("%s(%p): %d\n", __PRETTY_FUNCTION__, ip, __LINE__ );
    // *(ip->st): Result
    // *(ip->st - 1) : msg
    // *(ip->st - 2) : receiver
    // ip->name : sender  

    /*	printf("Builtin: attach\n");
	printf("receiver = ");
	print_term(ip->name_base, *(ip->st - 2));
	printf("\nmessage = ");
	print_term(ip->name_base, *(ip->st - 1));
	printf("\nresult = ");
	print_term(ip->name_base, *(ip->st));
	printf("\n");
	fflush(stdout);
    */
    // check caller's type

    if (!is_messenger(ip)){
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_BUILTIN;
	return;
    }

    // check receiver's name
    deref(t, ip->st - 2);
    if (!is_con(t)) { // t must be symbolic name
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    }
	
    t = con_value(t);
    if (t < ASCII_SIZE ){
	tname[0] = (char) t;
	tname[1] = '\0';
	name = tname;
    } else {
	name = name_ptr(ip->name_base, t);
    }

    // check Result

    deref(t, ip->st);
    if (!is_var(t)) { // t must be var
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    }
 retry:
    log_lock();
    if( (log = log_find( ip->queen.host_id, ip->application, name)) != NULL ) {	
	rip = log->ip;
	lstatus = log->status;
	strcpy( lname, log->server.name );/** No overflow possible... */
    } else {
	log = NULL;
	rip = NULL;
    }
    log_unlock();
    /** There are four options to cover.
	1) Receiver totally unknown:     log == NULL, rip == NULL, lstatus == x -> Bind to mip->queen.name
	2) Receiver gone:                log != NULL, rip == NULL, lstatus == LOG_MOVED -> Bind to log->server.name
	3) Receiver dead:                log != NULL, rip == NULL, lstatus == LOG_DISPOSED -> Bind to 'deceased'
	4) Receiver is on this machine:  log != NULL, rip != NULL -> attach to receiver
    */
    if( log == NULL && rip == NULL ) {	/** Case 1: Totally unknown. */
	/** Two cases:
	    1) The messenger IS NOT on the queen machine -> binding to 'moved(ip->queen.name)'
	    2) The messenger IS on the queen machine
	    2.a) The application still exist -> put in waiting_msger_queue
	    2.b) The application is no longuer -> destroy the messenger
	*/
	if( ip->queen.host_id != gethostid_cache() ) {
	    // binding to 'moved(ip->queen.name)'
	    v = search_index("moved", ip, ip->st);
	    *(ip->st + 1) = make_fun(v + ASCII_SIZE, 1);
	    v = search_index((ip->queen).name, ip, ip->st + 1);
	    *(ip->st + 2) = make_con(v + ASCII_SIZE);
	    deref(t, ip->st);
	    *(int*) t = make_str(ip->st + 1);
	    bt_trail(t, ip);
	    ip->st += 2;
	} else {
	    log_lock();
	    if( log_find( ip->queen.host_id, ip->application, NULL) ) {
		log_unlock();
		/** The application still exist. */
		ip->status = IMAGO_WAITING;
		icb_put_q( waiting_msger_queue, ip );
	    } else {
		/** The application is no longer... */
		ip->status = IMAGO_ERROR;
		ip->nl = E_ILLEGAL_MESGER;
	    }
	    log_unlock();
	}
    } else if( log != NULL && rip == NULL && lstatus == LOG_MOVED ) { /** Case 2: Receiver gone. */
	// binding ip->st to 'moved(log->server.name)'
	// first make a structure on top of stack
	// *(st + 1) <= moved/1 | FUN
	// *(st + 2) <= name | CON
	// *(st) <= (st + 1) | STR
	// ** NOTE** this may be modified wrt host address
	
	v = search_index( "moved", ip, ip->st );
	*(ip->st + 1) = make_fun(v + ASCII_SIZE, 1 );
	v = search_index( lname, ip, ip->st + 1 );
	*(ip->st + 2) = make_con( v + ASCII_SIZE );
	deref( t, ip->st );
	*(int*) t = make_str( ip->st + 1 );
	bt_trail( t, ip );
	ip->st += 2;
    } else if( log != NULL && rip == NULL && lstatus == LOG_DISPOSED ) { /** Case 3: Receiver dead. */
	// binding ip->st to 'deceased'
	v = search_index("deceased", ip, ip->st);
	deref(t, ip->st);
	*(int*) t = make_con(v + ASCII_SIZE);
	bt_bbtrail(t, ip);
    } else if( log != NULL && rip != NULL ) { /** Case 4: Receiver alive */
	icb_lock( rip );
	/** May be the imago is gone already. */
	if( rip->status == IMAGO_INVALID ) {
	    icb_unlock( rip );
	    goto retry;
	}
	ip->status = IMAGO_ATTACHING;  // set status to suspend the messenger
	/** For system messengers, things are just a little different...
	    We do not attach the messenger into the queen's msg queue,
	    instead, we reply to the messenger that it has found the queen.
	*/

	if( !is_sysmsger(ip) ) {
	    // should insert this messenger into rip's msq
	    msq = &rip->msq;
	    que_lock( msq );
	    if( rip->status == IMAGO_WAITING ) icb_insert( msq, ip ); // to front
	    else icb_append( msq, ip );   // to back
	    que_unlock( msq );
		
	    /** If that guy was waiting, wake him up. */
	    if( rip->status == IMAGO_WAITING ) {
		rip->status = IMAGO_READY;
		icb_put_signal(engine_queue, rip);
	    }
	    debug("ATTACH: attached to '%s' queue len = %d\n",
		  name, icb_length(msq));
	}
	icb_unlock( rip );
	/** Incomming Messenger: Update for short cuts. 
	    System messenger still require those short cut to 
	    be created as well. 
	    But no short cuts will be made if the messenger has 
	    been dispatched from the stationary host.
	*/
	if( ip->queen.host_id != ip->citizenship.host_id ) {
	    log_lock();
	    /** First, we look for a log entry of the source imago. */
	    /** If we find it, make sure it is not on this machine. */
	    if( (log = log_find(ip->queen.host_id, ip->application, ip->name)) != NULL ) {
		if( log->status == LOG_MOVED ) {
		    /** Make sure this is the very latest information. */
		    if( log->jumps < ip->jumps ) {
			/** Update the source. 
			    A messengers carry the jump count of its creator. */
			log_moved( log, ip->jumps, ip->citizenship.name );
		    }
		}
	    } else {
		/** If we did not find it, it means the worker never got that far off...
		    If that is a worker, create the short cut using the messenger. */
		log_moved( log_alive( NULL, ip ), ip->jumps, ip->citizenship.name );
		log_unlock();
		/** Also, because this is the apearence of a new worker, we need to release all 
		    messenger that are waiting for unknown worker. May be, the worker will be known
		    now... */
		release_waiting_msger();
	    }
	    log_unlock();
	}
    }
    return; 
}

void bp_char_code( ICBPtr ip ) { return; }

void bp_clone(ICBPtr ip){
    // clone(imago_name, Result)

    int t;
    printf("Imago '%s' clones itself.\n", ip->name );
    if (ip->type == STATIONARY){
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_CLONE;
	return;
    }

    deref(t, ip->st);
    if (!is_var(t)){
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    }
    if (ip->type == WORKER){ // do not check this for messengers
	deref(t, ip->st - 1); // check imago name
	if (!is_con(t)){
	    ip->status = IMAGO_ERROR;
	    ip->nl = E_UNKNOWN_IMAGO;
	    return;
	}
    }
    ip->status = IMAGO_CLONING;
    return;
}
void bp_close1( ICBPtr ip ) { return; }
void bp_close2( ICBPtr ip ) { return; }

void bp_create(ICBPtr ip){

    // create(file_name, imago_name, argument)

    int t;

    // *(ip->st): argument
    // *(ip->st - 1) : imago_name
    // *(ip->st - 2) : file_name

    if (ip->type != STATIONARY){
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_CREATE;
	return;
    }

    // check file name
    deref(t, ip->st - 2);
    if (!is_con(t)){
	ip->status = IMAGO_ERROR;
	ip->nl = E_UNKNOWN_IMAGO;
	return;
    }

    // check imago name
    deref(t, ip->st - 1);
    if (!is_con(t)){
	ip->status = IMAGO_ERROR;
	ip->nl = E_UNKNOWN_IMAGO;
	return;
    }

    ip->status = IMAGO_SYS_CREATION;
    return;
}


void bp_dispatch(ICBPtr ip){

    // dispatch(messenger_name, receiver, msg)

    int t;

    // *(ip->st): msg
    // *(ip->st - 1) : receiver
    // *(ip->st - 2) : messenger_name

    if (is_messenger(ip)){
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_CREATE;
	return;
    }

    // check messenger type: sys-defined (starting with a $)
    // or user-defined.

    deref(t, ip->st - 2);
    if (!is_con(t)) { // t must be symbolic name
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_MESGER;
	return;
    }
	
    ip->nl = E_SUCCEED; // E_SUCCEED represents sys-defined messenger
    t = con_value(t);
    if (t < ASCII_SIZE || *name_ptr(ip->name_base, t) != '$')
	ip->nl= E_ILLEGAL_MESGER; // represent user-defined messenger

    if (ip->type == WORKER && ip->nl == E_ILLEGAL_MESGER){
	ip->status = IMAGO_ERROR;
	return;
    }

    ip->status = IMAGO_DISPATCHING;
    // before return, if (ip->nl == E_SUCCEED) then dispatch a sys-defined
    // otherwise, load a user-defined messenger
    return;
}

void bp_dispose(ICBPtr ip){ 
    debug( "**********************************************************\n");
    debug( "    APPLICATION = %d\n", ip->application);
    if (is_sysmsger(ip))
	debug( "        System Messenger of %s has finished\n", ip->name);
    else if (is_messenger(ip))
	debug( "        Messenger of %s has finished\n", ip->name);
    else
	debug( "        Worker Imago %s has finished\n", ip->name);
    debug( "        Total memory expansion = %d, stack size = %d\n",
	   ip->expansions, ip->st - ip->stack_base);
    debug( "        Total garbage collection = %d, trail size = %d\n",
	   ip->collections, ip->block_ceiling - (int*) ip->tt);
    debug( "**********************************************************\n");
    ip->status = IMAGO_DISPOSE;
    ip->nl = E_SUCCEED;
    return; 
}

void bd_search_engine(ICBPtr ip){ return; }
void bp_abort(ICBPtr ip){ return; }
void bp_abs(ICBPtr ip){ return; }
void bp_add_counter(ICBPtr ip){ return; }
void bp_arccos(ICBPtr ip){ return; }
void bp_arcsin(ICBPtr ip){ return; }
void bp_arctan(ICBPtr ip){ return; }
void bp_arithmetic(ICBPtr ip){ return; }
void bp_asm(ICBPtr ip){ return; }
void bp_asm_call(ICBPtr ip){ return; }
void bp_asm_code_1(ICBPtr ip){ return; }
void bp_asm_code_3(ICBPtr ip){ return; }
void bp_asm_code_4(ICBPtr ip){ return; }
void bp_asm_label(ICBPtr ip){ return; }
void bp_asm_list(ICBPtr ip){ return; }
void bp_asm_resolve(ICBPtr ip){ return; }
void bp_asserta(ICBPtr ip){ return; }
void bp_assertz(ICBPtr ip){ return; }
void bp_atom(ICBPtr ip){ return; }
void bp_atom_chars( ICBPtr ip ) { 
    // atom_chars(+atom, ?list) or atom_chars(-atom, +list)
    // convert an atom to a list of chars, or reverse
    /**
     * TO DO:
     */

    return; 
}

void bp_atom_concat(ICBPtr ip) { 
    /** Reference: PROLOG, Part 1, General Core, Committee Draft, chapter 8.16.2, page 86.
     * atom_concat(+con,+con,-var)      - To concatenate two atoms into one.
     * atom_concat(+con,+con,+con)    - To check if both inputs sum up to the output.
     * atom_concat(+con,-var,+con)      - To extract the trailing part of the atom string.
     * atom_concat(-var,+con,+con)      - To extract the leading part of the atom string. 
     */
    int *st = ip->st;
    int v;

    register int r3 = *st--;
    register int r2 = *st--;
    register int r1 = *st--;
    register int l1 = 0;
    register int l2 = 0;
    register int l3 = 0;
    register char *c1 = NULL;
    register char *c2 = NULL;
    register char *c3 = NULL;

    bt_deref(r1);
    bt_deref(r2);
    bt_deref(r3);

    /** We do not support 'atom_concat(-,-,?)' */
    if( is_var(r1) && is_var(r2) ) 
	goto illegal;
    /** Cannot support 'atom_concat(-,+,-)' or 'atom_concat(+,-,-)' */
    if( (is_var(r1) || is_var(r2)) && is_var(r3) ) 
	goto illegal;
    /** If r1, r2 or r3 are not variables, they must be constants. */
    if( (!is_var(r1) && !is_con(r1)) || (!is_var(r2) && !is_con(r2)) || (!is_var(r3) && !is_con(r3)) ) 
	goto illegal;
    /** Type checking finished and passed successfully. */

    /** Gather the size of all involved strings. */
    if( is_con(r1) ) {
	c1 = name_ptr(ip->name_base, con_value(r1));
	l1 = strlen( c1 );
    }
    if( is_con(r2) ) {
	c2 = name_ptr(ip->name_base, con_value(r2));
	l2 = strlen( c2 );
    } 
    if( is_con(r3) ) {
	c3 = name_ptr(ip->name_base, con_value(r3));
	l3 = strlen( c3 );
    }
    /** */
	
    /** atom_concat(+const,+const,?) */
    if( is_con(r1) && is_con(r2) ) {
	if( is_con(r3) ) {
	    /** atom_concat(+const,+const,+const) */
	    /** Check the size for obvious miss-match. */
	    if( (l1+l2) != l3 ) goto fail;
	    /** Compare the leading of r1. */
	    if( memcmp(c1, c3, l1) ) goto fail;
	    /** Compare the trailling of r2. */
	    if( memcmp(c2, &c3[l3-l2], l2) ) goto fail;
	    /** Both match. */
	    return;
	} else {
	    /** atom_concat(+const,+const,-var) */
	    /** Concatenate the two strings. */
	    char buf[l1+l2+1];
	    memcpy( buf, c1, l1 );
	    memcpy( &buf[l1], c2, l2 );
	    /** Buf is now (c1 . c2) */
	    buf[l1+l2] = 0x00; /** Terminate the string. */
	    /** Store the return value. */
	    v = search_index(buf, ip, ip->st);
	    if( ip->nl ) {
		/** Name table has been expanded. */
		r3 = *ip->st;
		bt_deref(r3);
	    }
	    *(int*) r3 = make_con(v + ASCII_SIZE);
	    bt_bbtrail(r3, ip);
	    /** Good to go. */
	    return;
	}
	/** never reached. */
	goto illegal;
    }
    /** r3 must be a constant otherwise, we would not reach that point. 
	Either r1 or r2 is a variable, so let us gather find the match. */
    if( is_con(r1) ) {
	/** r2 is a variable. */
	/** Compare the leading of r1. */
	if( memcmp(c1, c3, l1) ) goto fail;
	/* Extract and instanciate r2. */
	/** Store the return value. */
	v = search_index(&c3[l1], ip, ip->st);
	if( ip->nl ) {
	    /** Name table has been expanded. */
	    r2 = *(ip->st-1);
	    bt_deref(r2);
	}
	*(int*) r2 = make_con(v + ASCII_SIZE);
	bt_bbtrail(r2, ip);
	/** All set. */
	return;
    } else {
	char buf[l3-l2+1];
	/** r1 must be variable. */
	/** Compare the trailling of r2. */
	if( memcmp(c2, &c3[l3-l2], l2) ) goto fail;
	/** Copy r1 part. */
	memcpy( buf, c3, l3-l2 );
	buf[l3-l2] = 0x00;
	/** Store the return value. */
	v = search_index(buf, ip, ip->st);
	if( ip->nl ) {
	    /** Name table has been expanded. */
	    r1 = *(ip->st-2);
	    bt_deref(r1);
	}
	*(int*) r1 = make_con(v + ASCII_SIZE);
	bt_bbtrail(r1, ip);
	/** All good to go. */
	return;
    }
    /** Not reached. */
    return; 
 illegal:
    ip->status = IMAGO_ERROR;
    ip->nl = E_ILLEGAL_ARGUMENT;
    return;
 fail:
    ip->status = IMAGO_BACKTRACK;
    return;
}
void bp_atom_length( ICBPtr ip ) { return; }
void bp_atom_printf(ICBPtr ip){ return; }
void bp_atomic(ICBPtr ip){ return; }
void bp_ceiling(ICBPtr ip){ return; }
void bp_clam_notrace(ICBPtr ip){ return; }
void bp_clam_trace(ICBPtr ip){ return; }
void bp_clear_style(ICBPtr ip){ return; }
void bp_clisting1(ICBPtr ip){ return; }
void bp_codegen_debug(ICBPtr ip){ return; }
void bp_codegen_nodebug(ICBPtr ip){ return; }
void bp_compile(ICBPtr ip){ return; }
void bp_compound( ICBPtr ip ) { return; }
void bp_consult(ICBPtr ip){ return; }
void bp_cos(ICBPtr ip){ return; }
void bp_counter_value(ICBPtr ip){ return; }
void bp_csh(ICBPtr ip){ return; }
void bp_ctime(ICBPtr ip){ 
    /** 
     *	ctime(-T)
     *
     * T will be instanciated to a float representing the current time with the 
     * real part been seconds and the fractional part been fractions of second.
     * This will be consistent with the sleep/1 predicate.
     */
    register int r1;
    struct timeval tv;
    struct timezone tz;
    double td;

    /** Dereference this variable. */
    deref(r1, ip->st);
    /** Must be a variable. */
    if( !is_var(r1) ) {
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    }
    /** Grab the time of day. */
    gettimeofday( &tv, &tz );
    /** Converte it to a float. */
    td = (double)((double)(tv.tv_sec) + (((double)tv.tv_usec) / 1000000.0));
    /** Build up the float on top of the stack. */
    *(int*)r1 = make_float( td, ip->st );
    /** Adjust the stack top. */
    ip->st += 3;
    /** Trail that guy. */
    bt_bbtrail(r1, ip);
    /** All set to go. */
    return; 
}

void bp_current_host(ICBPtr ip){ 
    char cname[MAX_DNS_NAME_SIZE];
    int t, v;
    /** Grab the current host name. */
    get_imago_localhost( cname, MAX_DNS_NAME_SIZE );
    /** Binding ip->st to the current host name. */
    v = search_index(cname, ip, ip->st);
    deref(t, ip->st);
    /** Must be a variable. */
    if( !is_var(t) ) {
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    } 
    *(int*) t = make_con(v + ASCII_SIZE);
    bt_bbtrail(t, ip);
}
void bp_current_input( ICBPtr ip ) { return; }
void bp_current_output( ICBPtr ip ) { return; }
void bp_database_search(ICBPtr ip) { return; }
void bp_debugging(ICBPtr ip){ return; }
void bp_dump2(ICBPtr ip){ return; }
void bp_dump3(ICBPtr ip){ return; }
void bp_dump_tableaus(ICBPtr ip){ return; }
void bp_dynamic(ICBPtr ip){ return; }
void bp_e_stats(ICBPtr ip){ return; }
void bp_edit(ICBPtr ip){ return; }
void bp_eqeq(ICBPtr ip){ return; }
void bp_exp(ICBPtr ip){ return; }
void bp_fasserta(ICBPtr ip){ return; }
void bp_fassertz(ICBPtr ip){ return; }
void bp_float(ICBPtr ip){ return; }
void bp_floor(ICBPtr ip){ return; }
void bp_flush(ICBPtr ip) { fsync( ip->cout ); }
void bp_flush_output( ICBPtr ip ) { return; }
void bp_flush_output1( ICBPtr ip ) { return; }
void bp_fork(ICBPtr ip){ return; }
void bp_functor1(ICBPtr ip){ return; }
void bp_functor3(ICBPtr ip){ /* T, B and S */

    // functor(r1, r2, r3) 	=> 	(-term, +atomic, +integer)
    //		    	=>	(+term, ?atomic, ?integer)
 
    int *st = ip->st;

    register int r3 = *st--;
    register int r2 = *st--;
    register int r1 = *st--;
    register int r4;

    bt_deref(r1);
    bt_deref(r2);
    bt_deref(r3);

    if (is_var(r1)){
	if (!is_cst(r2) || !is_int(r3)){
	    ip->status = IMAGO_ERROR;
	    ip->nl = E_ILLEGAL_ARGUMENT;
	    return; 
	}
 
	r3 = int_value(r3);
	if (r3 > MAX_ARITY){
	    ip->status = IMAGO_ERROR;
	    ip->nl = E_ILLEGAL_ARGUMENT;
	    return; 
	}
 
	if (r3 == 0){
	    *(int*) r1 = r2;	// atom
	    bt_bbtrail(r1, ip);
	    return;
	}
	else if (r2 == con_list){
	    st++; *st = (int) (st);		// set head unbound
	    *(int*) r1 = make_list(st);	// assign to var
	    bt_trail(r1, ip);
	    st++; *st = (int)st;// set tail unbound
	    ip->st = st;
	    return;
	}
	else {
	    *(++st) = make_fun(con_value(r2), r3);
	    *(int*) r1 = make_str(st);
	    bt_trail(r1, ip);
	    while (r3--){			// r3 unbound variables
		st++; *st = (int) (st);
	    }
	    ip->st = st;
	    return;
	}
    }
    else if (is_list(r1)){
	r4 = con_list;			// set r4 list-functor
	r1 = 2;				// set r1 list-arity
    }
    else if (is_str(r1)){
	r1 = *addr(r1);			// get functor
	r4 = make_con(fun_value(r1));	// set r4 str-functor
	r1 = arity(r1);			// set r1 str-arity
    }
    else {
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return; // error("Functor/3: arg1 a strcuture/list expected", "");
    }
    if (is_var(r2)){
	*(int*) r2 = r4;
	bt_bbtrail(r2, ip);
    }
    else if (r2 != r4) {			// functors donot agree
	ip->status = IMAGO_BACKTRACK;
	return;
    }
    if (is_var(r3)){
	*(int*) r3 = make_int(r1);
	bt_bbtrail(r3, ip);
    }
    else if (int_value(r3) != r1){		// arities donot agree
	ip->status = IMAGO_BACKTRACK;
	return;
    }
    return;
}
void bp_get_byte( ICBPtr ip ) { return; }
void bp_get_byte2( ICBPtr ip ) { return; }
void bp_get_char( ICBPtr ip ) { return; }
void bp_get_char2( ICBPtr ip ) { return; }
void bp_get_code( ICBPtr ip ) { return; }
void bp_get_code2( ICBPtr ip ) { return; }

void bp_ground(ICBPtr ip){ return; }
void bp_halt(ICBPtr ip){ return; }
void bp_hide(ICBPtr ip){ return; }
void bp_history(ICBPtr ip){ return; }
void bp_history1(ICBPtr ip){ return; }
void bp_implicit(ICBPtr ip){ return; }
void bp_integer( ICBPtr ip ) { return; }
void bp_integer_float( ICBPtr ip ) { return; }
void bp_length_2(ICBPtr ip){ /* B */
    // length(r1, r2) :	(+List, ?V)

    register int r2 = *(ip->st);
    register int r1 = *(ip->st - 1);
    register int r3;

    bt_deref(r1);
    bt_deref(r2);

    if (is_nil(r1)) r3 = 0;
    else if (is_list(r1)) {
	r3 = 1;
	while (1){
	    r1 = *(addr(r1) + 1);		// get tail
	    bt_deref(r1);
	    if (is_list(r1)) r3 += 1;
	    else break;
	}
    }
    else { // arg1 is not a list or []
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return; 
    }	 

    if (is_var(r2)){
	*(int*) r2 = make_int(r3);
	bt_bbtrail(r2, ip);
	return;
    }
    else if (is_int(r2) && int_value(r2) == r3){
	return;
    }
    else{  // arg2 is not a var or int
	ip->status = IMAGO_BACKTRACK;
	return; 
    }	 
}

void bp_listing(ICBPtr ip){ return; }
void bp_listing1(ICBPtr ip){ return; }
void bp_load(ICBPtr ip){ return; }
void bp_log(ICBPtr ip){ return; }
void bp_main(ICBPtr ip){ return; }
void bp_minus(ICBPtr ip){ return; }
void bp_more(ICBPtr ip){ return; }

void bp_move(ICBPtr ip){ 
    // move(server)
    int v;
    deref( v, ip->st );
    if( !is_con(v) ) {
	/** This is very wrong ! ! ! */
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
    } else
	ip->status = IMAGO_MOVE_OUT;
    return;
}
void bp_new_constant(ICBPtr ip){ return; }
void bp_nl(ICBPtr ip) { dprintf( ip->cout, "\n" ); }
void bp_no_implicit(ICBPtr ip){ return; }
void bp_nonground(ICBPtr ip){ return; }
void bp_nonvar(ICBPtr ip){ return; }
void bp_nospy(ICBPtr ip){ return; }
void bp_nospy2(ICBPtr ip){ return; }
void bp_notrace(ICBPtr ip){ return; }
void bp_number( ICBPtr ip ) { return; }
void bp_number_chars( ICBPtr ip ) { 
    // number_chars(+integer, ?list) or number_chars(-atom, +list)
    // convert a number to a list of chars, or reverse
    register int r1;
    register int r2;

    deref( r2, ip->st );
    deref( r1, ip->st - 1 );
    /**
     * TO DO:
     */







    return; 
}
void bp_occurs(ICBPtr ip){ return; }
void bp_op3(ICBPtr ip){ return; }
void bp_open3( ICBPtr ip ) { return; }
void bp_open4( ICBPtr ip ) { return; }
void bp_partial_implicit(ICBPtr ip){ return; }
void bp_peek_byte( ICBPtr ip ) { return; }
void bp_peek_byte2( ICBPtr ip ) { return; }
void bp_peek_char( ICBPtr ip ) { return; }
void bp_peek_char2( ICBPtr ip ) { return; }
void bp_peek_code( ICBPtr ip ) { return; }
void bp_peek_code2( ICBPtr ip ) { return; }
void bp_pipe(ICBPtr ip){ return; }
void bp_pow(ICBPtr ip){ return; }
void bp_prot(ICBPtr ip){ return; }
void bp_put_byte( ICBPtr ip ) { return; }
void bp_put_byte2( ICBPtr ip ) { return; }
void bp_put_char( ICBPtr ip ) { return; }
void bp_put_char2( ICBPtr ip ) { return; }
void bp_put_code( ICBPtr ip ) { return; }
void bp_put_code2( ICBPtr ip ) { return; }
void bp_rand(ICBPtr ip){ return; }
void bp_reconsult(ICBPtr ip){ return; }
void bp_replace_arg(ICBPtr ip){ return; }

void bp_reset_sender(ICBPtr ip){ 
    // *(ip->st): a constant (symbolic name) to replace the sender's name
    // must be used by messenger
    int t;

    //	printf("BUILTIN: reset_sender/1\n");
    //	fflush(stdout);

    if (is_messenger(ip)){
	deref(t, ip->st);
	if (is_con(t)){	
	    t = con_value(t);
	    strcpy(ip->name, name_ptr(ip->name_base, t));
	}
	else {
	    ip->status = IMAGO_ERROR;
	    ip->nl = E_ILLEGAL_ARGUMENT;
	}
    }
    else {
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_BUILTIN;
    } 
    return;

}

void bp_retractall(ICBPtr ip){ return; }
void bp_round(ICBPtr ip){ return; }
void bp_s_stats(ICBPtr ip){ return; }
void bp_see(ICBPtr ip){ return; }
void bp_seeing(ICBPtr ip){ return; }
void bp_seen(ICBPtr ip){ return; }

void bp_sender(ICBPtr ip){ /* B */
    // *(ip->st): variable to bind the sender's name
    // must be used by messenger
    int v, t;
 
    //	printf("BUILTIN: sender/1\n");
    //	fflush(stdout);

    if (is_messenger(ip)){
	deref(t, ip->st);
	if (is_var(t)){	
	    v = search_index(ip->name, ip, ip->st);
	    *(int*) t = make_con(v + ASCII_SIZE);
	    bt_bbtrail(t, ip);
	    return;
	}
	else {
	    ip->status = IMAGO_ERROR;
	    ip->nl = E_ILLEGAL_ARGUMENT;
	}
    }
    else {
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_BUILTIN;
    } 
    return; 
}

void bp_set_counter(ICBPtr ip){ return; }
void bp_set_input( ICBPtr ip ) { return; }
void bp_set_output( ICBPtr ip ) { return; }
void bp_set_stream_position( ICBPtr ip ) { return; }
void bp_sh(ICBPtr ip){ return; }
void bp_sin(ICBPtr ip){ return; }
void bp_sleep(ICBPtr ip) {
  // sleep/1  
  struct timeval tv, tv1;
  struct timezone tz;
  int v;
  
  /** In a worry of precision, the dday is computed here
      and will not be recomputed in the imago_timer module. */
  timerclear( &tv1 );
  /** Grab the amount of time the imago desires to sleep. */
  /** First we dereference the stack pointer so we can get the integer value. */
  deref( v, ip->st );
  /** Then, follow the link to the value, either int or float. */
  if( is_int(v) ) {
    /** Grab the integer value and store it as a double (without fractionnal value) */
    tv1.tv_sec = int_value(v);
  } else if( is_float(v) ) {
    double d = float_value( v );
    tv1.tv_sec = (int)d;
    /** fractionnal part in microsecond (10e6). */
    tv1.tv_usec = (int)((d - (double)tv1.tv_sec) * 1000000.0);
  } else {
    /** Neither int nor float means instanciation error. */
    ip->status = IMAGO_ERROR;
    ip->nl = E_ILLEGAL_ARGUMENT;
    return;
  }
  gettimeofday( &tv, &tz );
  /** Otherwise, compute the dday when this imago will have to be waken up... */
  timeradd(&tv, &tv1, &ip->dday );
  /** In any case, we give this imago to the imago_timer module. */
  ip->status = IMAGO_TIMER;
  
}
void bp_spy(ICBPtr ip){ return; }
void bp_spy2(ICBPtr ip){ return; }
void bp_sqrt(ICBPtr ip){ return; }
void bp_srand(ICBPtr ip){ return; }
void bp_stationary_host(ICBPtr ip) {
    int t, v;
    /** Binding ip->st to 'received'. */
    v = search_index(ip->queen.name, ip, ip->st);
    deref(t, ip->st);
    /** Must be a variable. */
    if( !is_var(t) ) {
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    } 
    *(int*) t = make_con(v + ASCII_SIZE);
    bt_bbtrail(t, ip);
}
void bp_stats(ICBPtr ip){ return; }
void bp_style_check(ICBPtr ip){ return; }
void bp_sub_atom( ICBPtr ip ) { return; }


void bp_syscall( ICBPtr ip ) {
    LOGPtr log;
    int t, v;
    char *name, tname[2];
    char *cmd, tcmd[2];

    debug("BUILTIN: syscall/1\n");
    /** Make sure this is a system messenger. */
    if( !is_sysmsger(ip) ) {
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_BUILTIN;
	return;
    }

    /** We call bp_attach to allow the messenger to be forwarded accordingly. */
    bp_attach( ip );
    /** Now, we know that is the messenger has fond its receiver, the bp_attach will switch the
	messenger status to IMAGO_ATTACHING. In that case, we can just execute the syscall...
    */
    if( ip->status != IMAGO_ATTACHING ) 
	return;
    /** Retreive the system call it is carying. */
    deref(t, ip->st - 1);
    if( !is_con(t) ) {	/** Must be a constant. */
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    }
    t = con_value(t);
    if (t < ASCII_SIZE ){
	tcmd[0] = (char) t;
	tcmd[1] = '\0';
	cmd = tcmd;
    } else {
	cmd = name_ptr(ip->name_base, t);
    }
    /** */    

    /** Get receiver */
    deref(t, ip->st - 2);
    if (!is_con(t)) { // t must be symbolic name
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    }
    t = con_value(t);
    if (t < ASCII_SIZE ){
	tname[0] = (char) t;
	tname[1] = '\0';
	name = tname;
    } else {
	name = name_ptr(ip->name_base, t);
    }
    /** */
    
    debug( "SysCall=%s\n", cmd );
    
    if( strcmp("deceased", cmd) == 0 ) {
	/** A worker died. */
	/** Find this guy's log entry. */
	log_lock();
	log = log_find( ip->queen.host_id, ip->application, ip->name );
	if( log ) {
	    /** We found the entry. */
	    log_disposed( log );
	} else {
	    /** No such worker ??? 
		May be this application has terminated earlier. */
	    if( log_find( ip->queen.host_id, ip->application, NULL ) ) {
		/** If the application is still up and running, register this guy as already dead. */
		log_disposed( log_alive(NULL, ip) );
	    } 
	}
	log_unlock();
    } else if( strcmp("kill", cmd) == 0 ) {
	ICBPtr tip = NULL;
	/** Find this guy's log entry. */
	log_lock();
	/** If we find him (most of the cases), kill him down. */
	if( (log = log_find( ip->queen.host_id, ip->application, name )) != NULL )
	    tip = log->ip;
	log_unlock();    
	/** */
	/** Hummmm..... If that guy slipt away, we will have missed the chance to kill him... */
	if( tip ) {
	    /** WARNING WARNING WARNING WARNING WARNING WARNING 
	     *  If for some reason, that guy slipted away before we got a chance to lock it down
	     * then, we are screwed trying to kill some dead body......
	     * WARNING WARNING WARNING WARNING WARNING WARNING */
	    icb_lock( tip );
	    if( tip->status == IMAGO_INVALID ) {
		/** That is a furious one ! ! ! :) */
		notice("Worker Imago '%s' escaped the kill command from '%s'...\n",
		       name, ip->name );
	    } else {
		/** Safely dispose of it. */
		dispose_of( tip );
	    }
	    icb_unlock( tip );
	} 
    } else if( strcmp("clone", cmd) == 0 ) {
	/** A new worker is born... */
    } else {
	ip->status = IMAGO_ERROR;
	ip->nl = E_ILLEGAL_ARGUMENT;
	return;
    }

    /** Binding ip->st to 'received'. */
    v = search_index("received", ip, ip->st);
    deref(t, ip->st);	
    *(int*) t = make_con(v + ASCII_SIZE);
    bt_bbtrail(t, ip);

    ip->status = IMAGO_READY;
}

void bp_tan(ICBPtr ip){ return; }
void bp_tell(ICBPtr ip){ return; }
void bp_telling(ICBPtr ip){ return; }

void bp_terminate(ICBPtr ip){
    debug( "**********************************************************\n");
    debug( "    APPLICATION = %d\n", ip->application);
    debug( "        Stationary Imago %s has finished\n", ip->name);
    debug( "        Total memory expansion = %d, stack size = %d\n",
	   ip->expansions, ip->st - ip->stack_base);
    debug( "        Total garbage collection = %d, trail size = %d\n",
	   ip->collections, ip->block_ceiling - (int*) ip->tt);
    debug( "**********************************************************\n");
    ip->status = IMAGO_TERMINATE;
    ip->nl = E_SUCCEED;
    return; 
}

void bp_told(ICBPtr ip){ return;}
void bp_trace(ICBPtr ip){ return; }
void bp_truncate(ICBPtr ip){ return; }

void bp_univ(ICBPtr ip){ return; }
void bp_var(ICBPtr ip){ return; }

void bp_wait_accept(ICBPtr ip){ 
    // wait_accept(Sender, Msg)
    int ret;
    if( (ret = messenger_accept(ip)) == 0 ) {
	/** No messenger received. */
	ip->status = IMAGO_WAITING;  
	/** update the log status. */
	log_lock();
	ip->log->status = LOG_BLOCKED;
	log_unlock();
    } else if( ret > 0 ) {
	/** We received a messenger. */
	ip->status = IMAGO_READY;
    } else if( ip->status == IMAGO_EXPANSION ) {
	/** We may need to expand the worker because the messenger carried a huge message. */
    } else {
	/** There was an error. */
	ip->status = IMAGO_ERROR;
    }
 
    // this status makes the scheduler ingoring this imago, and
    // schedules another ready imago
    // however, we must reset program counter such that this
    // predicate will be re-executed when this imago become
    // READY again, this is done by engine

    return;
}

void bp_warning(ICBPtr ip){ return; }
void bp_wasm(ICBPtr ip){ return; }
void bp_web_search(ICBPtr ip) { return; }
void bp_workers(ICBPtr ip) {
    /**
     *  My original proposal is as follow:
     *
     *          workers(Template, Status, L)
     *
     *  where Template is a string with wild chars such as ? and *.
     *
     *  ?: match one character
     *  *: match one or more characters
     *
     *  and Status indicates the matching status of workers, such as alive,
     *  disposed, or blocked, and L is a variable to hold collected workers.
     *
     *  For example, 
     *
     *          worker("node*", alive, L)
     *
     *  ==> collect workers whose name is nodeX...X and they are "alive" at the 
     *      current host.
     *
     *          worker(_, _, L)
     *
     *  ==> collect all workers in this application at the current host.
     *
     *          worker(_, disposed, L)
     *
     *  ==> a list of disposed workers at the current host.
     *
     *
     *  etc.
     *
     *  The advantage of such arrangement is that we may collect a list of 
     *  workers which satisfy the Template and then implement multi_casting.
     */
    /** We implement the builtin as 'workers(+const,+const,-list)' */
    register int r3 = *(ip->st);
    register int r2 = *(ip->st - 1);
    register int r1 = *(ip->st - 2);
    int *st = ip->st + 1;
    int status = -1, preg_c = 0;
    LOGPtr log;
    regex_t preg;
    memset( &preg, 0, sizeof(preg) );

    bt_deref(r1);
    bt_deref(r2);
    bt_deref(r3);
    
    /** The last argument MUST be an uninstanciated variable. */
    if( (!is_con(r1) && !is_var(r1)) || (!is_con(r2) && !is_var(r2)) || !is_var(r3) ) {
	/** Not a list... */
	goto err;
    }
    /** Get the status required. */
    if( is_con(r2) ) {
	char *arg2 = name_ptr(ip->name_base, con_value(r2));
	if( arg2[0] == 'a' && !strcmp(arg2, "alive") ) {
	    /** Only alive workers. */
	    status = LOG_ALIVE;
	} else if( arg2[0] == 'm' && !strcmp(arg2, "moved") ) {
	    /** Only alive workers. */
	    status = LOG_MOVED;
	} else if( arg2[0] == 'd' && !strcmp(arg2, "disposed") ) {
	    /** Only alive workers. */
	    status = LOG_DISPOSED;
	} else if( arg2[0] == 'b' && !strcmp(arg2, "blocked") ) {
	    /** Only alive workers. */
	    status = LOG_BLOCKED;
	} else {
	    /** unknown status.. */
	    goto err;
	}
    }
    /** Start looking for the matching status, and if we find one matching status, then compile the regex and use it. */
    log_lock();
    for( log = live_logs->head; log; log = log->next ) {
	if( (log->server.host_id == ip->queen.host_id) && 
	    (log->application == ip->application) && 
	    ((status < 0) || log->status == status) ) {
	    int v;
	    /** Ok, premilinary test have been passed. 
		Now we need to check for the regex. */
	    if( is_con(r1) ) {
		/** Compile the regex. */
		if( !preg_c && regcomp(&preg, name_ptr(ip->name_base, con_value(r1)), REG_NOSUB) ) {
		    /** Error in the expression, or out-of-memeory. */
		    goto err;
		}
		preg_c = 1;
		if( regexec(&preg, log->name, 0, NULL, 0) ) {
		    /** This is not a match. */
		    continue;
		}
	    }

	    /** Make the list element. */
	    *st = make_list( st + 1 );
	    st++;
	    /** Add the name into the name table. */
	    v = search_index( log->name, ip, st );
	    if( ip->nl ) {
		/** The name table has been expanded... We are in trouble... */
		st += ip->nl;
	    }
	    /** Add the element into the list. */
	    *st++ = make_con( v + ASCII_SIZE );
	}
	/** Go to the next entry. */
    }
    log_unlock();
    if( preg_c ) 
	regfree(&preg);
    /** End the list. */
    *st = con_nil;
    /** Bind the result value with the newly created functor. */
    *(int*) r3 = make_ref(ip->st + 1);
    bt_trail(r3, ip);
    ip->st = st;
    return; 
 err:
    ip->status = IMAGO_ERROR;
    ip->nl = E_ILLEGAL_ARGUMENT;
    return;
}
void bp_write(ICBPtr ip){ print_term( ip->cout, ip->name_base, *ip->st ); }
void bp_write2( ICBPtr ip ) { return; }
void bp_ztime(ICBPtr ip){ return; }

/****************************************************************/
/**       ! ! ! PLEASE, KEEP THE ALPHA ORDERING ! ! !           */
/****************************************************************/

/** Helper function section. */

int match_messenger(ICBPtr ip, ICBPtr mip){ /* T, B */

    int rt, v, rs, ms, *rst, *mst;
    char *name, tname[2];

    // ip: receiver's ICB
    // *(ip->st): Msg
    // *(ip->st - 1): Sender

    // mip: messenger's ICB
    // *(mip->st - 1) : msg
    // mip->name: message sender

    // get receiver-side sender name
    deref(rt, ip->st - 1);
    if (is_var(rt)){
	name = NULL;
    }
    else if (!is_con(rt)) { // t must be symbolic name
	return E_ILLEGAL_ARGUMENT;
    }
    else { // make name points to the symbolic name
	rt = con_value(rt);
	if (rt < ASCII_SIZE ){
	    tname[0] = (char) rt;
	    tname[1] = '\0';
	    name = tname;
	}
	else{
	    name = name_ptr(ip->name_base, rt);
	}
    }

    // now, name is either NULL (var) or a pointer to sender expected
    // rt must be retained for later name binding in the case of var

    if (!name || strcmp(name, mip->name) == 0){ // the right one
	// match msg first

	// get receiver-side msg
	deref(rs, ip->st);

	// get messenger-side msg
	deref(ms, mip->st - 1);	
 
	// now we have four cases:
	// 0) rs and ms are both var: return
	// 1) rs is a var: copy ms's stuff to rs
	// 2) ms is a var: copy rs's stuff to ms
	// 3) neither rs nor ms is a var: matching and possibly cross copying

	if (is_var(rs)){
	    if (!is_var(ms)){
		rst = copy_arg(ms, mip->name_base, ip); 
		/** If the copy_arg failed, we need to get this imago to expand and try again. */
		if( rst == NULL ) return ip->nl;
		deref(rs, ip->st); // deref again in case of
		*(int*) rs = *rst; // name_table expansion
		// trail rs in ip's space
		bt_trail(rs, ip);			
		ip->st = rst;
	    }
	}
	else if (is_var(ms)){
	    mst = copy_arg(rs, ip->name_base, mip); 
	    /** If the copy_arg failed, we need to get this imago to expand and try again. */
	    if( mst == NULL ) return mip->nl;
	    deref(ms, mip->st - 1); // deref again in case of
	    *(int*) ms = *mst; // name_table expansion
	    // trail ms in mip's space
	    bt_trail(ms, mip);		
	    mip->st = mst;
	}
	else { // need full unification
	    rst = copy_arg(ms, mip->name_base, ip); 
	    /** If the copy_arg failed, we need to get this imago to expand and try again. */
	    if( rst == NULL ) return ip->nl;
	    mst = copy_arg(rs, ip->name_base, mip);
	    /** If the copy_arg failed, we need to get this imago to expand and try again. */
	    if( mst == NULL ) return mip->nl;

	    // unify msg on receiver space
	    if (msg_unify(ip->st, rst, ip) != E_SUCCEED){ 
		return E_UNKNOWN_IMAGO;
	    }
	    ip->st = rst;			// set new stack top
	    // unify msg on messenger space
	    // we do not check error because it must succeed
	    msg_unify(mip->st - 1, mst, mip); 
	    mip->st = mst;			// set new stack top
	}

	// binding sender's name if rt is a var

	if (!name){
	    v = search_index(mip->name, ip, ip->st);
	    *(int*) rt = make_con(v + ASCII_SIZE);
	    bt_bbtrail(rt, ip);
	}	
	return E_SUCCEED;
    }
    else return E_UNKNOWN_IMAGO;
}


int msg_unify(int* t1, int* t2, ICBPtr ip){ /* T and B */
    // t1: pointer to expected msg
    // t2: pointer to the copied msg, also the actual top of ip's stack.
    //     	which is higher than ip->st, if unif succeeds, ip->st must be set to t2.
    // 	We do not modify ip->st if unif fails which seems that the copied msg 
    //	not existed and all side-effects have been removed, however, constants 
    //	inserted (during copying) in name_table can not be eliminated.
    // *** here we use the old trail, bbtrail filter_trail and clear_trail
    //     defined in engine.h

    int ax, bx, *st, *gl, *bb, **tt, *gp, flag;

    /****************************************************************/
    /* The following code implement general unification:            */
    /* a) prepare ax and bx                                         */
    /* b) st holds stack top                                        */
    /* c) in case of nonunifiable, flag <- MLVM_NONE                */
    /* d) in case of unifiable, flag <- MLVM_ZERO                   */
    /* e) this code will use t2 = st as the temprory stack.         */
    /****************************************************************/

    gl = ip->gl; // used in trail macro
    bb = ip->bb; // used in btrail macro
    tt = ip->tt; // used in trailing/detraling
    gp = (int*) tt; // used in filter/clear trail operations

    ax = *t1;
    bx = *t2;
    st = t2;
    flag = MLVM_ZERO;

    while(1){
	// dereferencing ax and bx

    MAXDEF:	  if (is_var(ax)){
	if (ax != *(int*)(ax)){
	    ax = *(int*) (ax);
	    goto MAXDEF;
	}
	else{
	    *(int*) ax = bx;
	    if (is_cst(bx)){
		bbtrail(ax);
	    }
	    else {
		trail(ax);
	    }

	    if (st == t2){
		break;	// succeed
	    }
	    else {
		ax = *(st--);
		bx = *(st--);
		continue;
	    }
	}
    }

    MBXDEF:   if (is_var(bx)){
	if (bx != *(int*)(bx)){
	    bx = *(int*) (bx);
	    goto MBXDEF;
	}
	else {
	    *(int*) bx = ax;
	    if (is_cst(ax)){
		bbtrail(bx);
	    }
	    else {
		trail(bx);
	    }

	    if (st == t2){
		break;	// succeed
	    }
	    else {
		ax = *(st--);
		bx = *(st--);
		continue;
	    }
	}
    }

    if (ax != bx){
	if (is_list(ax) && is_list(bx)){
	    *(++st) = *(addr(bx) + 1);
	    *(++st) = *(addr(ax) + 1);
	    bx = *addr(bx);
	    ax = *addr(ax);
	    continue;
	}
	else if (is_float(ax) && is_float(bx)){
	    if (*(addr(ax) + 1) != *(addr(bx) + 1) ||
		*(addr(ax) + 2) != *(addr(bx) + 2)){
		flag = MLVM_NONE;
		break;
	    }
	    if (st == t2){
		break;	// succeed
	    }
	    else {
		ax = *(st--);
		bx = *(st--);
		continue;
	    }
	}
	else if (is_str(ax) && is_str(bx)){

	    int x; 
	    int y; 
	    // get str_term address
	    derefstr(x, ax);
	    derefstr(y, bx);
	    
	    ax = *addr(x);	// get functor
	    bx = *addr(y);

	    if (ax != bx) {
		flag = MLVM_NONE;
		break;
	    }

	    ax = arity(ax);

	    while (ax > 1) { // push arguments
		*(++st) = *(addr(y)+ax);
		*(++st) = *(addr(x)+ax);
		ax--;
	    }
	    bx = *(addr(y)+1);
	    ax = *(addr(x)+1);
	    continue;
	}
	else {
	    flag = MLVM_NONE;
	    break;
	}
    }
    else {
	if (st == t2){
	    break;	// succeed
	}
	else {
	    ax = *(st--);
	    bx = *(st--);
	    continue;
	}
    }
    }

    if (flag == MLVM_ZERO){
	// if succeed, we have to check trails which
	// were set during unification. Some of them
	// are below bb and some of them are above bb.
	// all bindings above bb will be deleted

	filter_trail();
	ip->st = t2;  // set up new stack top
	ip->tt = tt;
	return E_SUCCEED;
    }
    else{
	clear_trail();
	return E_NO_SOLUTION;
    }
}
int messenger_accept(ICBPtr ip){ 
    // wait_accept(Sender, Msg)
    // accept(Sender, Msg)
    
    int v, t, *tmp;
    icb_queue* msq;
    ICBPtr mip;

    msq = &(ip->msq);
    que_lock(msq);

 retry:
    // search for a matching messenger
    for( mip = icb_head(msq); mip; mip = icb_next(mip) ) {
	tmp = mip->st;		// save old top
	v = match_messenger( ip, mip );
	if( v == E_SUCCEED ) {
	    ip->status = IMAGO_READY;
	    // remove mip from msq
       	    icb_remove(msq, mip);
	    que_unlock(msq);

	    log_lock();	  
	    ip->log->status = LOG_ALIVE;
	    log_unlock();	    
    
	    // we should also set mip "received"
	    // and set mip READY to continue
	    
	    v = search_index("received", mip, mip->st);
	    deref(t, tmp);	// save return value to the old top
	    *(int*) t = make_con(v + ASCII_SIZE);
	    bt_bbtrail(t, mip);
	    mip->status = IMAGO_READY;
 
	    icb_put_signal(engine_queue, mip);
	    return 1;
	} else if( v == E_ILLEGAL_ARGUMENT ) {
	    ip->status = IMAGO_ERROR;
	    ip->nl = v;
	    que_unlock(msq);
	    return -1;
	} else if( v == IMAGO_EXPANSION ) {
	    /** Outch ! One of the two Imago need to be expanded because its stack
		was not big enough to hold the entire message. */
	    /** If the worker should be expanded, make him do the expansion, then restart the accept
		procedure again (simple case). */
	    if( ip->nl == IMAGO_EXPANSION ) {
		ip->status = IMAGO_EXPANSION;
		ip->nl = v;
		que_unlock(msq);
		debug( "In %s: Worker '%s' should be expanded\n", 
		       __PRETTY_FUNCTION__, mip->name );

		return -1;
	    } else {
		/** If the messenger should be expanded, take him out of the queue, roll back its 'pp'
		    and put him into the memory expansion queue so it can be grown and do the attach again
		    (not so simple case). */
		// remove mip from msq
		icb_remove(msq, mip);
		/** Roll back the 'pp'. A builtin is two integer long instruction. */
		mip->pp -= 2;
		/** Say it should expand. */
		mip->status = IMAGO_EXPANSION;
		/** Make him expand. */
		icb_put_signal(memory_queue, mip);
		debug( "In %s: Messenger from '%s' should be expanded\n", 
		       __PRETTY_FUNCTION__, mip->name );
		/** retry the matching a messenger. */
		goto retry;
	    }
	}
    }
    debug("%s(%p): No messenger for %s (msq length: %d)\n", 
	  __PRETTY_FUNCTION__, ip, ip->name, icb_length(&ip->msq) );

    que_unlock(msq);
    // if we come to here, then msq either empty or 
    // there is no matching messenger,
    // make the worker WAITING (waiting on the log queue)
    // until later a newly attached messenger releases
    // it to READY
    return 0;
}


/** THE END */


