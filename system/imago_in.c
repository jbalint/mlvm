/** $RCSfile: imago_in.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_in.c,v 1.22 2003/03/21 13:12:03 gautran Exp $ 
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
/*	 	  imago_in.c                    */
/************************************************/

#include "system.h"
#include "transport.h"
extern int mlvm_stop;
/************************************************/
/** This is the callback function called        */
/** whenever some Imago arrives here.           */
/************************************************/
void imago_in_cb( const char *from, ICBPtr ip ) {
    debug( "A new Imago just moved in (ip=%p, name=%s, jumps=%d, queen name=%s, queen host id=0x%lX, citizenship=%s).\n", 
	   ip, ip->name, ip->jumps, ip->queen.name, ip->queen.host_id, ip->citizenship.name );
    /** Put the newly received ICB into the in_queue. */
    icb_put_signal(in_queue, ip);
}

/************************************************/
/* This is a system thread responsible for      */
/* receiving and handling incoming messages,    */
/* such as control info and moving imagoes.     */
/************************************************/


int imago_in(){ // this is temporary implementation
    ICBPtr ip;

    icb_get(in_queue, ip);
    
    if( ip == NULL || ip->status == IMAGO_INVALID )
	return 0;

    debug("pthread[%ld]: %s(ip=%p) \n", pthread_self(), __PRETTY_FUNCTION__, ip);
    // check imago status
  
    if( ip->status == IMAGO_MOVE_IN || ip->status == IMAGO_ERROR ) {
	/** We received a new Imago from a distant planet or the one we launched came right back into our faces. */
	if( !is_messenger(ip) ) {
	    /** Retreive the log entry for this incoming Imago. */
	    if( ip->status != IMAGO_ERROR )
		ip->jumps++;
	    
	    log_lock();
	    log_alive( log_find(ip->queen.host_id, ip->application, ip->name), ip );
	    log_unlock();
	}
	/** If the error flag is set, the imago try to move out but could not.
	    In that case, the move predicate fails and we need to backtrack (0/1 semantic). */
	if( ip->status == IMAGO_ERROR ) 
	    ip->status = IMAGO_BACKTRACK;    
	else 
	    ip->status = IMAGO_READY;
	/** The server is shutting down and flushing out all Imago. */
	if( mlvm_stop )
	    ip->status = IMAGO_TERMINATE;
	/** If the imago is home, restore the file streams. */
	if( ip->queen.host_id == gethostid_cache() ) {
	    LOGPtr log;
	    ICBPtr q_ip;
	    log_lock();
	    /** The queen is always the first in the log entry since it is the one created first. */
	    log = log_find(ip->queen.host_id, ip->application, NULL );
	    if( log ) q_ip = log->ip;
	    log_unlock();
	    if( q_ip ) {
		icb_lock( q_ip );
		if( q_ip->status != IMAGO_INVALID ) {
		    ip->cin  = q_ip->cin;
		    ip->cout = q_ip->cout;
		    ip->cerr = q_ip->cerr;
		}
		icb_unlock( q_ip );
	    }
	    /** release waiting messengers. */
	    release_waiting_msger();
	}
	/* awake engine */
	icb_put_signal(engine_queue, ip);
    } else {
	/** We cannot handle anything else... */
	icb_put_signal(out_queue, ip);
    }
	
    return 1;
}




#if 0

void * imago_in(void* data){ // this is temporary implementation
    int socket_id, rstat;
    struct sockaddr_in insock,from;
    int fromlen;
    ICBPtr ip;
    ICB buf;
 
    printf("imago_in %ld: \n", pthread_self());

    /** Transport protocol requires us to register the callback function... */
    transport_register_callback( imago_in_callback );
    /** */

    socket_id=socket(AF_INET,SOCK_DGRAM,0);
    insock.sin_family=AF_INET;
    insock.sin_addr.s_addr= INADDR_ANY;
    insock.sin_port=PORTNUM;

    bind(socket_id, (struct sockaddr*)&insock, sizeof(insock));;

    fromlen = sizeof(from);

    while(1) { 
	rstat=recvfrom(socket_id, &buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);

	printf("imago_in received file: %s\n", buf.name);

	ip = icb_new();      // get a new icb
	if (!ip){
	    system_error("Imago_in", E_ICB_IN);
	    exit(-1);
	}
	strcpy(ip->name, buf.name);
	ip->status = IMAGO_CREATION;

	// initialize tracing vars, snum will be read 
	ip->call_seq = 0;
	ip->start_num = buf.start_num;
	ip->call_count = 0;
	ip->mid_call = 0;

  	
	/* awake creator */

	icb_put_signal(creator_queue, ip);

    }
}
#endif
