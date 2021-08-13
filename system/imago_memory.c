/** $RCSfile: imago_memory.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_memory.c,v 1.10 2003/03/21 13:12:03 gautran Exp $ 
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
/*	 	  imago_memory.c                    */
/************************************************/

#include "system.h"

/************************************************/
/* A system thread responsible for memory       */
/* expansion and garbage collection.            */
/************************************************/

int mmanager(ICBPtr);

int imago_memory() {
    ICBPtr ip;
    int r;

    // get an imago 
    icb_get(memory_queue, ip);

    if( ip == NULL || ip->status == IMAGO_INVALID )
	return 0;
  
    debug("pthread[%ld]: %s(ip=%p) \n", pthread_self(), __PRETTY_FUNCTION__, ip);
    /** If the system is shutting down, we terminate all non messenger imago. */
    if( mlvm_stop && !is_messenger(ip) ) {
	ip->status = IMAGO_TERMINATE;
	icb_put_signal(out_queue, ip);
	return 1;
    }

    r = mmanager(ip);
  
    // call memory manager and check result
    if (r == E_SUCCEED) {
	ip->status = IMAGO_READY;
	/* awake engine */
	icb_put_signal(engine_queue, ip);
    } else if (r == E_MEMORY_OUT){  
	// memory out: to be insert to wait_for_memory queue
	/** If the server is going down, terminate this guy. */
	if( mlvm_stop ) {
	    ip->status = IMAGO_TERMINATE;
	    /** awake engine */
	    icb_put_signal( out_queue, ip );
	} else {
	    debug("imago_memory: insert to waiting_mem_queue\n");
	    icb_put_q( waiting_mem_queue, ip );
	}
    } else if( r == E_RESSOURCE_BUSY ) {  
	debug("imago_memory: ressources busy.\n");
	/** Someone was using the ressources, just wait for a little bit. */
	icb_put( memory_queue, ip );
	/** Say we have not done anything... */
	return 0;
    } else {
	ip->status = IMAGO_ERROR;
	ip->nl = r;
	icb_put_signal(out_queue, ip);
    } 
    return 1;
}
