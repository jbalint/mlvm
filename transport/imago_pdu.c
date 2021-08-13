/** $RCSfile: imago_pdu.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_pdu.c,v 1.15 2003/03/24 14:07:48 gautran Exp $ 
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "include.h"
#include "queue.h"
#include "frame.h"
#include "imago_pdu.h"
#include "imago_signal.h"


/** Private member variables section. */

#ifdef _REENTRANT
pthread_mutex_t pdu_table_lock = PTHREAD_MUTEX_INITIALIZER ;
#endif
static struct pdu_str pdu_table[IMAGO_MAX_PDU];
static unsigned int pdu_count = 0;
static unsigned int pdu_max_count = 0;
static struct link_head_str pdu_list_hdr;
/** Member methods section. */

void pdu_init( struct pdu_str *new_pdu ) {
    /** Clear the structure. */
    memset( new_pdu, 0, sizeof(struct pdu_str) );
    /** The new PDU does not have its self pointer pointing to
	itself. */
    new_pdu->self = new_pdu;
    /** Initialize the flags. */
    new_pdu->flags |= PDU_TYPE_SITP;
    /** Initialize the frame pointer queue. */
    queue_init( &new_pdu->frames );
    /** Ready to go ! */  
}

struct pdu_str *pdu_new( ) {
    int i;
    /** The max number of PDU is fixed. So scan the array so we can find one free. */
    struct pdu_str *new_pdu = NULL;
#ifdef _REENTRANT
    /** First, lock the table so we do not clash with the destroy function. */
    pthread_mutex_lock( &pdu_table_lock );
#endif 
    for( i = 0; i < IMAGO_MAX_PDU; i++ ) {
	/** An invalid PDU does not have its "self" pointing to itself. */
	if( pdu_table[i].self != &pdu_table[i] ) {
	    new_pdu = &pdu_table[i];
	    /** Initialize the PDU. */
	    pdu_init( new_pdu );
	    break;
	}
    }
    /** If we did not run out of PDUs... */
    if( i < IMAGO_MAX_PDU ) {
	/** Increment the pdu counter. */
	pdu_count++;
	/** Update the max PDU counter. */
	if( pdu_count > pdu_max_count )
	    pdu_max_count = pdu_count;
	/** Logging... */
#ifdef _LOG_STATISTICS
	debug( "Creating a new imago_pdu (used: %d, free: %d, max: %d)\n", 
	       pdu_count, IMAGO_MAX_PDU-pdu_count,  pdu_max_count);
#endif
    }
#ifdef _REENTRANT
    /** All done, so unlock the mutex. */
    pthread_mutex_unlock( &pdu_table_lock );
#endif
    debug("%s(%p)\n", __PRETTY_FUNCTION__, new_pdu );
    /** Return our findings. */
    return new_pdu;
}

void pdu_free( struct pdu_str *lost_pdu ) {
    debug("%s(%p)\n", __PRETTY_FUNCTION__, lost_pdu );
    /** Just make sure it is a valid pointer... */
    if( !lost_pdu ) return;
#ifdef _REENTRANT
    /** First, lock the table so we do not clash with the destroy function. */
    pthread_mutex_lock( &pdu_table_lock );
#endif 
    /** First make sure this is a valid PDU. */
    if( lost_pdu->self == lost_pdu ) {
	/** To destroy a PDU, just clearout its 'self' pointer. */
	lost_pdu->self = NULL;
	/** Decrement the pdu counter. */
	pdu_count--;
	if( lost_pdu->flags & PDU_ERROR ) {
	    warning("PDU Error detected: %d at %d\n",
		    lost_pdu->errno, lost_pdu->e_layer );
	}
#ifdef _LOG_STATISTICS
	/** Logging some statistics on the room used for this guy... */
	debug( "Destroying an imago_pdu (used: %d, free: %d, frame used: %d)\n", 
	       pdu_count, IMAGO_MAX_PDU-pdu_count, frame_count(&lost_pdu->frames) );
#endif
	/** We need to free all the frames. */
	frame_destroy( &lost_pdu->frames );
	/** Signal if the pdu number is decreasing. */
	if( pdu_count == (IMAGO_MAX_PDU-1) ) imago_signal_kill( SIGIO );
    } else {
	warning( "Freeing an invalid PDU.\n" );
    }
#ifdef _REENTRANT
    /** All done, so unlock the mutex. */
    pthread_mutex_unlock( &pdu_table_lock );
#endif
}

void pdu_enqueue( struct pdu_str *new_pdu ) {
    //    extern const int SIGrestart;
    //    int restart = size(&pdu_list_hdr);
    //  debug( "%s(%p)\n", __PRETTY_FUNCTION__, new_pdu ); 
    enqueue( &pdu_list_hdr, &new_pdu->link );
    /** Signal everybody that something has been added to the list... */
    //    if( !restart )
    //    	killpg( 0, SIGrestart );
}

struct pdu_str *pdu_dequeue( ) {
    //  debug( "%s()\n", __PRETTY_FUNCTION__ ); 
    return (struct pdu_str *)dequeue( &pdu_list_hdr );
}
void pdu_set_state( struct pdu_str *this_pdu, state_id_t stateid ) {
    /** Just set the specified state on this PDU. */
    this_pdu->state.state = stateid;
}

unsigned int pdu_size( register struct pdu_str *this_pdu ) {
    return frame_size( &this_pdu->frames ); 
}

int pdu_send_frame( int sd, register struct pdu_str *this_pdu ) {
    return frame_send( sd, &this_pdu->frames );
}
int pdu_erase_frame( register struct pdu_str *this_pdu, const int l ) {
    return frame_erase( &this_pdu->frames, l );
}




/** these functions are used to read/write a string/int/short/byte to the PDU header. */
int pdu_write_text( register struct pdu_str *this_pdu, const char *te1 ) {
    return frame_write_text( &this_pdu->frames, te1 );
}
int pdu_write_string( register struct pdu_str *this_pdu, const char *st1, const int length ) {
    return frame_write_string( &this_pdu->frames, st1, length );
}
int pdu_write_int( register struct pdu_str *this_pdu, int v1 ) {
    return frame_write_int( &this_pdu->frames, v1 );
}
int pdu_write_short( register struct pdu_str *this_pdu, short s1 ) {
    return frame_write_short( &this_pdu->frames, s1 );
}
int pdu_write_byte( register struct pdu_str *this_pdu, char c1 ) {
    return frame_write_byte( &this_pdu->frames, c1 );
}

int pdu_read_text( register struct pdu_str *this_pdu, char *te1, int max ) {
    return frame_read_text( &this_pdu->frames, te1, max );
}

int pdu_read_string( register struct pdu_str *this_pdu, char *st1, const int length ) {
    return frame_read_string( &this_pdu->frames, st1, length );
}
int pdu_read_int( register struct pdu_str *this_pdu, int *v1 ) {
    return frame_read_int( &this_pdu->frames, v1 );
}
int pdu_read_short( register struct pdu_str *this_pdu, short *s1 ) {
    return frame_read_short( &this_pdu->frames, s1 );
}

int pdu_read_byte( register struct pdu_str *this_pdu, char *c1 ) {
    return frame_read_byte( &this_pdu->frames, c1 );
}
/** */
struct pdu_str *pdu_clone( const struct pdu_str *o_pdu, struct pdu_str *c_pdu ) {
    struct pdu_str *t_pdu = c_pdu;
    debug("%s(%p,%p)\n", __PRETTY_FUNCTION__, o_pdu, c_pdu );
    /** If the clone pdu is provided, free all frames it may have. */
    if( t_pdu == NULL ) t_pdu = pdu_new();
    else frame_destroy( &t_pdu->frames );
    /** Still, make sure we do have something to load to. */
    if( t_pdu == NULL ) return NULL;
    /** First, copy the source PDU to the destination PDU. */
#ifdef _REENTRANT
    /** We HAVE to lock the PDU table because the restored structure will be not valid until its self
	pointer really pointes to itself. */
    pthread_mutex_lock( &pdu_table_lock );
#endif 
    memcpy( t_pdu, o_pdu, sizeof(struct pdu_str) );
    t_pdu->self = t_pdu;
#ifdef _REENTRANT
    pthread_mutex_unlock( &pdu_table_lock );
#endif 
    /** The initialize some pointers. */
    queue_init( &t_pdu->frames );
    /** And get ready to copy all the frames from one PDU to the clone. */
    if( frame_copy(&t_pdu->frames, &o_pdu->frames, frame_size(&o_pdu->frames)) < 0 ) {
	/** If the copy failed, destroy all the partial frames. */
	frame_destroy( &t_pdu->frames );
	if( c_pdu == NULL ) pdu_free( t_pdu );
	t_pdu = NULL;
    }
    /** All good to go. */
    /** The clone is in t_pdu. */
    return t_pdu;
}


/** THE END */

#if 0
struct pdu_str *pdu_forward( struct pdu_str *this_pdu ) {
    /** Make sure this is a valid PDU. */
    if( !this_pdu || this_pdu->self != this_pdu ) 
	return NULL;
    /** Move the PDU UP or DOWN according to the flags. */
    if( IMAGO_PDU_FLAG(this_pdu) & IMAGO_PDU_FLAG_OUTGOING ) {
	/** Moving DOWN the stack (out). */
	this_pdu->type--;
    } else  if( IMAGO_PDU_FLAG(this_pdu) & IMAGO_PDU_FLAG_INCOMMING ) {
	/** Moving UP the stack (in). */
	this_pdu->type++;
    } else {
	/** We don't know where this one is going. */
	pdu_free( this_pdu );
	this_pdu = NULL;
    }
    /** Return what we were working on. */
    return this_pdu;
}

struct pdu_str *imago_pdu_backoff( struct pdu_str *this_pdu ) {
    /** Make sure this is a valid PDU. */
    if( !this_pdu || this_pdu->self != this_pdu ) 
	return NULL;
    /** Move the PDU UP or DOWN according to the flags. */
    if( IMAGO_PDU_FLAG(this_pdu) & IMAGO_PDU_FLAG_OUTGOING ) {
	/** Moving DOWN the stack (out). */
	this_pdu->type++;
    } else  if( IMAGO_PDU_FLAG(this_pdu) & IMAGO_PDU_FLAG_INCOMMING ) {
	/** Moving UP the stack (in). */
	this_pdu->type--;
    } else {
	/** We don't know where this one is going. */
	imago_pdu_destroy( this_pdu );
	this_pdu = NULL;
    }
    /** Return what we were working on. */
    return this_pdu;
}
#endif 
