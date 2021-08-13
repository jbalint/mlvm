/** $RCSfile: imago_out.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_out.c,v 1.26 2003/03/21 13:12:03 gautran Exp $ 
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
/*           imago_out.c                        */
/************************************************/

#include "system.h"
#include "opcode.h"
#include "engine.h"
#include "transport.h"

extern int imago_sys_dispatcher( ICBPtr );

/************************************************/
/* A system thread responsible for handling     */
/* out-going messages, such as control info     */
/* and moving imagoes.                          */
/************************************************/

int imago_out(){
    ICBPtr ip;
    LOGPtr log;

    icb_get(out_queue, ip);
  
    if( ip == NULL || ip->status == IMAGO_INVALID ) 
	return 0;

    debug("pthread[%ld]: %s(ip=%p) \n", pthread_self(), __PRETTY_FUNCTION__, ip);
    // check imago status
  
    switch (ip->status){
	caseop(IMAGO_MOVE_OUT): {
	    int v;
	    char to[MAX_DNS_NAME_SIZE];
	    /** First we dereference the stack pointer so we can get the integer value. */
	    deref( v, ip->st );
	    if( !is_con(v) ) {
		/** This is very wrong ! ! ! */
		ip->status = IMAGO_ERROR;
		ip->nl = E_ILLEGAL_ARGUMENT;
		icb_put_signal(out_queue, ip);
		break;
	    }
	    /** Then, follow the link to the symbolic name. */
	    v = con_value(v); 
	    if (v < ASCII_SIZE){
		/** The machine may have a one character name size. */
		to[0] = (char) v;
		to[1] = '\0';
	    } else { 
		/** Check if the destination is ourself. */
		get_imago_localhost( to, MAX_DNS_NAME_SIZE );
		if( strcasecmp(to, name_ptr(ip->name_base, v)) == 0 ) {
		    /** We are looping back. */
		    ip->status = IMAGO_MOVE_IN;
		    icb_put_signal( in_queue, ip );
		    break;
		} else if( strcasecmp(LOOPBACK_INTERFACE, name_ptr(ip->name_base, v)) != 0 ) {
		    /** Otherwise, we just do a string copy of the entire name. */
		    strncpy( to, name_ptr(ip->name_base, v), MAX_DNS_NAME_SIZE-1 );
		}
	    }
	    
	    /** Update the log. */
	    if (!is_messenger(ip)){
		log_lock();
		log = ip->log;
		log->status = LOG_MOVED;
		log->ip = NULL;
		/** the string copy is safe because the "to" buffer is safe (See above). */
		strcpy( log->server.name, to ); 
		time( &log->time );
		log_unlock();
	    }
		
	    debug( "Imago '%s' is moving out (ip=%p) to %s.\n", ip->name, ip, to  );
	    /** Then, send the imago. */
	    transport_send( to, ip );

	    /** here is the place to release all messengers in msq and let them know the worker has moved. */
	    if (!is_messenger(ip))
		release_msger( ip, to );
	    
	    break;
	}
	caseop(IMAGO_ERROR):   
	    caseop(IMAGO_DISPOSE): 	
	    caseop(IMAGO_TERMINATE): 
	    // send an error/terminate-report to stationary host
	    // and deallocate resources
	    // change log
	    debug("Imago_out:");
 
	if( !is_messenger(ip) ) {
	    if( ip->log ) {
		log_lock();
		log = ip->log;
		log->status = LOG_DISPOSED;
		log->ip = NULL;
		log_unlock();
	    }
	    // here is the place to release all messengers in msq
	    release_msger(ip, 0);

	    /** We need to notify the queen of the death of her child. */
	    imago_sys_dispatcher( ip );

	    debug(" imago");
	} else {
	    debug(" messenger of");
	}
	debug(" %s has been deallocated\n", ip->name);

	icb_free(ip);
	// as soon as one imago terminates
	// all of the waiting imago can be
	// released to compete for memory

	que_lock(waiting_mem_queue);
	while (que_not_empty(waiting_mem_queue)){
	    ip = icb_first(waiting_mem_queue);  
	    switch (ip->status){
		caseop(IMAGO_EXPANSION):
		    caseop(IMAGO_COLLECTION):
		    // insert ip to Memory manager's schedule queue
		    icb_put_signal(memory_queue, ip);
		break;
		caseop(IMAGO_MOVE_IN):
		    caseop(IMAGO_CLONING):
		    caseop(IMAGO_DISPATCHING):   
		    caseop(IMAGO_CREATION):	    
		    caseop(IMAGO_SYS_CREATION): 
		    // insert ip to Creator's schedule queue
		    icb_put_signal(creator_queue, ip);
		break;
	    default: 
		break;
	    }
	}
	que_unlock(waiting_mem_queue);
	break;
    
    default: 
      /** Not a valid status. */
      icb_put_signal( engine_queue, ip );
      break;
    }
    return 1;
}

