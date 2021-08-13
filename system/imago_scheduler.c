/** $RCSfile: imago_scheduler.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_scheduler.c,v 1.10 2003/03/21 13:12:03 gautran Exp $ 
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
/*	 	  imago_scheduler.c             */
/************************************************/

#include "system.h"

/************************************************/
/* A system thread responsible for scheduling   */
/* imagoes. It will get an imago's ICB from the */
/* ready queue and call engine to execute.      */
/************************************************/

void engine(ICBPtr);

int imago_scheduler(){
    ICBPtr ip;
  
    // get a ready imago
    icb_get(engine_queue, ip);

    if( ip == NULL || ip->status == IMAGO_INVALID ) 
	return 0;

    debug("pthread[%ld]: %s(ip=%p) \n", pthread_self(), __PRETTY_FUNCTION__, ip);
    /** If the system is shutting down, we terminate all non messenger imago. */
    if( mlvm_stop && !is_messenger(ip) ) {
	ip->status = IMAGO_TERMINATE;
	icb_put_signal(out_queue, ip);
	return 1;
    }

    // execute this imago 
    engine(ip);
  
    // return value from engine(ip) is stored in 
    // ip->status and ip->nl.
    // if (ip->status == IMAGO_ERROR) then ip->nl
    // gives the error code, but this will be handled
    // by the imago_out manager. 
    // otherwise, ip->status indicates the type
    // of context_switch.

    // check execution result
    switch (ip->status){
	caseop(IMAGO_READY):
	    /** Probably after a forced context switch point. */
	    icb_put_signal(engine_queue, ip);
	break;
	caseop(IMAGO_EXPANSION):
	    caseop(IMAGO_CONTRACTION):
	    caseop(IMAGO_COLLECTION):
	    // insert ip to Memory manager's schedule queue
	    icb_put_signal(memory_queue, ip);
	break;

	caseop(IMAGO_ERROR):     
	    caseop(IMAGO_MOVE_OUT):  
	    caseop(IMAGO_DISPOSE):   
	    caseop(IMAGO_TERMINATE): 
	    // insert ip to Out-manager's schedule queue
	    icb_put_signal(out_queue, ip);
	break;

	caseop(IMAGO_MOVE_IN):
	    caseop(IMAGO_CLONING):
	    caseop(IMAGO_DISPATCHING):   
	    caseop(IMAGO_CREATION):	    
	    caseop(IMAGO_SYS_CREATION): 
	    // insert ip to Creator's schedule queue
	    debug("SCHEDULE %s to create\n", ip->name);
	icb_put_signal(creator_queue, ip);
	break;

	caseop(IMAGO_ATTACHING):
	    caseop(IMAGO_WAITING):
	    //  the imago has been suspended, nothing to do
	    // schedule next ready imago
	    icb_unlock( ip );
	    break;

	caseop(IMAGO_TIMER):
	    // insert ip to Timer's queue
	    // to be decided
	    printf("imago_scheduler: insert to timer_queue\n");
	icb_put_signal(timer_queue, ip);
	break;

    default: 
	icb_unlock( ip );
	break; 
    }
    return 1;
}
