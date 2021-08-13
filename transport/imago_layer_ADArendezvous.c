/** $RCSfile: imago_layer_ADArendezvous.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_ADArendezvous.c,v 1.14 2003/03/24 14:07:48 gautran Exp $ 
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
/** ############################################################################
 *   ADA rendez-vous header
 *
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  -+ 
 * |        Generation ID          |    PDU Type   |    Reserved   |   |ADA 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   |header 
 * |                           Sequence ID                         |   | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  -+ 
 * |                                                               |   
 * /                            data                               /   
 * |                                                               |   
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   
 *  
 * 
 * ########################################################################## */


#include <stdio.h>
#include <string.h>

#include "include.h"
#include "imago_pdu.h"
#include "pdu_backup.h"
#include "state_machine.h"
#include "imago_layer_security.h"
#include "imago_layer_marshaling.h"
#include "imago_layer_ADArendezvous.h"

/** State machine ids. */
state_id_t imago_layer_ADArendezvous_up_stateid = -1;
state_id_t imago_layer_ADArendezvous_down_stateid = -1;
/** */

state_id_t imago_layer_ADArendezvous_up( struct pdu_str *);
state_id_t imago_layer_ADArendezvous_down( struct pdu_str *);
int dispatch_control_pdu( unsigned short, struct pdu_str * );

/** This variable does not need any locking.
    It is just used to indicate shutdown and will only be set to 1. */
static int adarendezvous_shutdown = 0;

/** ADA rendez-vous generation id and sequence id. */
static unsigned short generation = 0;
static unsigned int   sequence   = 0;
#ifdef _REENTRANT
pthread_mutex_t gen_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/** */

/** Timeout values. */
struct timeval tv_rcv = {
    tv_sec: ADA_ACK_TIMEOUT,
    tv_usec: 0,
};
struct timeval tv_snd = {
    tv_sec: ADA_ACCEPT_TIMEOUT,
    tv_usec: 0,
};
/** */



/** Initialization routine called once at the very first begining. */
void imago_layer_ADArendezvous_init() {
    /** Initialize the generation id number with the time of this machine. */
    generation = time(NULL) & 0xffff;

    /** Register the states in the state machine. */
    imago_layer_ADArendezvous_up_stateid = state_machine_register( (state_action_t)imago_layer_ADArendezvous_up );
    imago_layer_ADArendezvous_down_stateid = state_machine_register( (state_action_t)imago_layer_ADArendezvous_down );
    /** */
    /**
     * TO DO: Load the backups from the backup directory and then send the requests or the accept accordingly.
     */
}
/** */

/** Inform the layers and all other peers that the server is to terminate itself.
    Do not accept anymore requests... */
void imago_layer_ADArendezvous_shutdown() {
    adarendezvous_shutdown = 1;
}
/** */
int imago_layer_ADArendezvous_getopt( int opt , void *arg ) {
    return -1;
}
int imago_layer_ADArendezvous_setopt( int opt, void *arg ) {
    return -1;
}


/** Safely retreive sequence ID and generation ID. */
static unsigned short get_gen_id() {
    unsigned short gen;
#ifdef _REENTRANT
    pthread_mutex_lock( &gen_lock ); 
#endif
    gen = generation;
#ifdef _REENTRANT
    pthread_mutex_unlock( &gen_lock ); 
#endif
    return gen;
}

static unsigned int get_seq_id() {
    unsigned int seq;
#ifdef _REENTRANT
    pthread_mutex_lock( &gen_lock ); 
#endif
    seq = sequence++;
    /** Handle the wrap arround case. */
    if( sequence == 0 )
	generation = time(NULL) & 0xffff;
    /** */
#ifdef _REENTRANT
    pthread_mutex_unlock( &gen_lock ); 
#endif
    return seq;
}
/** */
void timeout_callback( timer_id id, void *arg ) {
    struct pdu_str *inc_pdu = NULL;
    debug( "%s(%d, %p)\n", __PRETTY_FUNCTION__, id, arg );
    /** This callback routine will be called whenever the timer id fires. 
	This means that either, 
	1) the REQUEST we sent did not get ACCEPT
	or 
	2) the ACCEPT packet we sent did not get ACK (or NACK).

	First we have to load the PDU back from storage so we can 
	find out which of those two cases apply.
    */
    /** The argument is a backup ID. */
    if( (inc_pdu = backup_idload( (int)arg, NULL )) == NULL ) {
	/** We did not find the backup. 
	    Just ignore this spurious alarm. */
	return;
    }
    /** We have restored the PDU from the backup. */
    if( inc_pdu->ADAtype & ADA_TYPE_REQUEST ) {
	/** We never received the ACCEPT packet. */
	/** Are we out of retries ?? */
	if( inc_pdu->retry >= ADA_MAX_RETRY ) {
	    inc_pdu->flags |= PDU_ERROR;
	    inc_pdu->errno = EPDU_NADA;
	    inc_pdu->e_layer = imago_layer_ADArendezvous_down_stateid;
	    pdu_set_state( inc_pdu, imago_layer_marshaling_up_stateid );
	    timer_free( id );
	} else {
	    /** If we can retry again, put this PDU back into the queue to be sent again. */
	    inc_pdu->ADAtype |= ADA_TYPE_RETRY;
	}
	/** Destroy this backup. it will be recreated later. */
	backup_idcancel( (int)arg ); 
	/** Queue this thing so it can get processed. */
	pdu_enqueue( inc_pdu );
    } else if( inc_pdu->ADAtype & ADA_TYPE_ACCEPT ) {
	/** Well... Our ACCEPT never got ACK or NACK. */
	/** Not much to do except send our ACCEPT again until we hear from the other side... */
	/** Dispatch a "accept" packet. */
	dispatch_control_pdu( ADA_TYPE_ACCEPT, inc_pdu );
	/** No matter if we have been able to dispatch the response
	    we still keep the backup in storage. 
	    If the dispatch packet did not go throught, the peer
	    will eventually retry.
	*/
	/** Start the timer. */
	timer_set( id, &tv_rcv, timeout_callback, arg );
	/** The old PDU is not required anymore... Destroy it. */
	pdu_free( inc_pdu );
    } else {
	/** What is that PDU ??? */
	/** Destroy this backup. */
	backup_idcancel( (int)arg ); 
	/** free the timer. */
	timer_free( id );
	/** Free the PDU. */
	pdu_free( inc_pdu );
    }
}

/** ############################################################################
 *   ADA rendez-vous header
 *
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  -+ 
 * |        Generation ID          |    PDU Type   |    Reserved   |   |ADA 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   |header 
 * |                           Sequence ID                         |   | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  -+ 
 * |                                                               |   
 * /                            data                               /   
 * |                                                               |   
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   
 *  
 * 
 * ########################################################################## */

int write_header( struct pdu_str *this_pdu ) {
    /** Write the ADArendez-vous header. */
    /** Last is the sequence ID. */
    if( pdu_write_int( this_pdu, this_pdu->seqid ) < 0 ) {
	this_pdu->errno = EPDU_MFRAME;
	return -1;
    }
    /** In the middle is the ADA request type (REQUEST). */
    if( pdu_write_short( this_pdu, this_pdu->ADAtype ) < 0 ) {
	this_pdu->errno = EPDU_MFRAME;
	return -1;
    }
    /** In first position is the generation ID. */
    if( pdu_write_short( this_pdu, this_pdu->genid ) < 0 ) {
	this_pdu->errno = EPDU_MFRAME;
	return -1;
    }
    return 0;
}
int read_header( struct pdu_str *this_pdu ) {
    /** Read the ADA header from the PDU. */
    pdu_read_short( this_pdu, &this_pdu->genid );
    pdu_read_short( this_pdu, &this_pdu->ADAtype  );
    pdu_read_int( this_pdu, &this_pdu->seqid );
    /** */
    return 0;
}
/** ############################################################################ 
 *
 * Dispatch a CONTROL PDU (SICP) responding to the same sequence id and 
 * generation id than the original pdu.
 *
 * ########################################################################## */
int dispatch_control_pdu( unsigned short type, struct pdu_str *this_pdu ) {
    struct pdu_str *control_pdu;
    /** Create the control PDU to send the Accept? packet. */
    if( (control_pdu = pdu_new()) == NULL ) {
	return -1;
    }
    /** Initialize it all. */
    /** Mark it as an SICP. */
    control_pdu->flags &= ~PDU_TYPE_SITP;
    /** Set the ADA information. */
    control_pdu->genid = this_pdu->genid;
    control_pdu->seqid = this_pdu->seqid;
    control_pdu->ADAtype  = type;
    /** Write the destination server to the PDU. */
    strcpy( control_pdu->destination_server, this_pdu->source_server );
    /** Set the state for this new pdu. */
    pdu_set_state( control_pdu, imago_layer_security_down_stateid );
    /** Write down the ADA header and send this pdu */
    if( write_header( control_pdu ) < 0 ) {
	/** Something is wrong. */
	pdu_free( control_pdu );
	return -1;
    }
    /** Once this is all set, push the pdu into the IO queue to be sent. */
    pdu_enqueue( control_pdu );
    /** Return a success. */
    return 0;
}
/** ######################################################################### */

/** ############################################################################
 *
 * When a TRANSPORT Packet is to be transmitted:
 *
 *  ADA PDU Type: REQUEST
 *    The corresponding PDU is stored and transmitted as a ADA REQUEST type 
 *   PDU. 
 *
 * ########################################################################## */
state_id_t imago_layer_ADArendezvous_down( struct pdu_str *this_pdu ) {
    char backup_key[BACKUP_NAME_MAX]; /** String should be long enough. */
    int b_id;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu );

    /** Initialize the ADA info. */
    if( !(this_pdu->ADAtype & ADA_TYPE_RETRY) ) {
	/** Get a new sequence ID as well as a the generation ID for this new PDU to send. */
	this_pdu->genid = get_gen_id();
	this_pdu->seqid = get_seq_id();
	this_pdu->ADAtype  = ADA_TYPE_REQUEST;
	this_pdu->timer    = timer_new();
	this_pdu->retry = 0;
    }
    this_pdu->retry++;
    /** Create a backup ID using the generation and sequence IDs. */
    snprintf( backup_key, sizeof(backup_key), ADA_PREFIX "%d.%d", this_pdu->genid, this_pdu->seqid );
    /** Backup this PDU. */
    if( (b_id = backup_save( backup_key, this_pdu )) < 0 ) {
	this_pdu->errno = EPDU_NBACK;
	goto error;
    }
    /** Write the PDU header down into the frames. */
    if( write_header( this_pdu ) < 0 )
	goto error;

    /** Start the timer. */
    timer_set( this_pdu->timer, &tv_snd, timeout_callback, (void*)b_id );
    /** 
     * TO DO: start an alarm timer so we can monitor when the ACCEPT packet is going to arrive. 
     */
    return imago_layer_security_down_stateid;
 error:
    /** Something went wrong. Abort and forward back the ICB to where it belongs. */
    this_pdu->flags |= PDU_ERROR;
    this_pdu->e_layer = imago_layer_ADArendezvous_down_stateid;
    /** The PDU timer may have been allocated. Free it. */
    timer_free( this_pdu->timer );
    /** And return an invalid state. */
    return imago_layer_marshaling_up_stateid;
}

/** ############################################################################
 *
 * When a CONTROL Packet is received:
 *
 *  ADA PDU Type: ACCEPT
 *     If the generation is older than the current one, send a NACK
 *     If same generation, the corresponding PDU is destroy and a CONTROL Packet 
 *   (ADA PDU Type ACK) is sent.
 *
 *  ADA PDU Type: ACK
 *    The corresponding PDU is restored and put back into the queue for the 
 *   marshaling layer (No error).
 *
 *  ADA PDU Type: NACK
 *    The corresponding PDU is restored and put back into the queue for the 
 *   marshaling layer (With error status set).
 *
 * ########################################################################## */
state_id_t imago_layer_ADArendezvous_control_up( struct pdu_str *this_pdu ) {
    /** Should be enough for the server name, generation ID and sequence ID. */
    char backup_key[BACKUP_NAME_MAX];
    struct pdu_str *inc_pdu;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
 
    /** Read the header. */
    if( read_header( this_pdu ) < 0 ) {
	/** Something is wrong with the ADA header. */
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    }
    /** What Packet type is this ??? */
    switch( this_pdu->ADAtype ) {
    case ADA_TYPE_ACCEPT:
	/** check the generation id. */
	if( this_pdu->genid != get_gen_id() ) {
	    /** If the generation is different from the current one, respond a NACK. */
	    dispatch_control_pdu( ADA_TYPE_NACK, this_pdu );
	    /** Leave it running in case the backup was stalled. */
	} else {
	    /** Create a backup ID using the generation and sequence IDs. */
	    snprintf( backup_key, sizeof(backup_key), ADA_PREFIX "%d.%d", this_pdu->genid, this_pdu->seqid );
	    /** Try to load the PDU. If it is in the backup still, it means it has not been discarded
		from the timeout just yet. */
	    /** 
	     * TO DO: They may be a PDU duplication right here.
	     *        If the ACCEPT packet arrives, and then the timer fires BEFORE we had time to free it
	     *   but AFTER we reloaded the backup, then it will be ACKed on one side and return as 
	     *   Timed out on the other...
	     */
	    if( (inc_pdu = backup_load( backup_key, NULL )) == NULL ) {
		/** You are too late buddy ! ! ! */
		dispatch_control_pdu( ADA_TYPE_NACK, this_pdu );
	    } else {
		/** Not too late. */
		/** Stop and free the timer. */
		timer_free( inc_pdu->timer ); 
		/** Our request has been accepted, send a ACK packet and destroy our backup. */
		dispatch_control_pdu( ADA_TYPE_ACK, this_pdu );
		/** Destroy the backup. */
		backup_cancel( backup_key );
		/** Destroy the PDU. */
		pdu_free( inc_pdu );
	    }
	}
	break;
    case ADA_TYPE_ACK:
	/** Our service offer (accept?) has been acknowledge. 
	    Insert the PDU for the unmarshaling layer. */
	snprintf( backup_key, sizeof(backup_key), ADA_PREFIX "%s:%d.%d", 
		  this_pdu->source_server, this_pdu->genid, this_pdu->seqid );
	if( (inc_pdu = backup_load( backup_key, NULL )) == NULL ) {
	    /** Big problem... */
	    /**
	     * TO DO: We lost the PDU... 
	     */
	    break;
	} else {
	    /** Stop and free the timer. */
	    timer_free( inc_pdu->timer ); 
	    backup_cancel( backup_key );
	}
	pdu_set_state( inc_pdu, imago_layer_marshaling_up_stateid );
	pdu_enqueue( inc_pdu );
	break;
    case ADA_TYPE_NACK:
	/** Our service offer (accept?) has not been acknowledge. 
	    Destroy the backup PDU. */
	snprintf( backup_key, sizeof(backup_key), ADA_PREFIX "%s:%d.%d", 
		  this_pdu->source_server, this_pdu->genid, this_pdu->seqid );
	if( (inc_pdu = backup_load( backup_key, NULL )) != NULL ) {
	    /** Stop and free the timer. */
	    timer_free( inc_pdu->timer ); 
	}
	backup_cancel( backup_key );
	pdu_free( inc_pdu );
	break;
    case ADA_TYPE_REQUEST:
    default:
	/** None of the above, this is not ours. */
	return imago_layer_marshaling_up_stateid;    
    }
    /** We do not need this PDU anymore... */
    pdu_free( this_pdu );
    return STATE_MACHINE_ID_INVALID;
}

/** ############################################################################
 *
 * When a TRANSPORT Packet is received:
 *
 *  ADA PDU Type: REQUEST (Error indicator not set)
 *    It is backed up, then a CONTROL Packet (ADA PDU Type ACCEPT) is sent.
 *
 *  ADA PDU Type: REQUEST (Error indicator set)
 *    Destroy the backup and forward it to the marshaling layer (internal error).
 *
 * ########################################################################## */
state_id_t imago_layer_ADArendezvous_transport_up( struct pdu_str *this_pdu ) {
    /** Should be enough for the server name, generation ID and sequence ID. */
    char backup_key[MAX_DNS_NAME_SIZE+50];
    int b_id;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 

    /** If the PDU is in error, try to recover. */
    if( this_pdu->flags & PDU_ERROR ) {
	/** Recover the original PDU from the backup. */
	/** The PDU may have been encrypted and totally screwd up. */
	/** Save the error code and place. */
	pdu_error_t p_errno = this_pdu->errno;
	state_id_t  p_e_layer = this_pdu->e_layer;
	/** Save the generation ID and sequence ID used to restore the PDU. */
	unsigned short gen_id = this_pdu->genid;
	unsigned int   seq_id = this_pdu->seqid;
	/** Then discard this PDU. */
	pdu_free( this_pdu );
	/** Load the backup from disk. */
	snprintf( backup_key, sizeof(backup_key), ADA_PREFIX "%d.%d", 
		  gen_id, seq_id );
	if( (this_pdu = backup_load(backup_key, NULL)) == NULL ) {
	    /** Could not load the PDU backup... :( */
	    error( "ERROR PDU lost (missing backup '%s').\n", backup_key );
	    return STATE_MACHINE_ID_INVALID;
	}
	/** We recovered the backup, so restore the error codes. */
	this_pdu->errno = p_errno;
	this_pdu->e_layer = p_e_layer;
	this_pdu->flags |= PDU_ERROR;
	/** Change the direction for this PDU (from going down to going up the stack). */
	pdu_set_state( this_pdu, imago_layer_marshaling_up_stateid );
	/** enqueue the PDU so it get processed. */
	pdu_enqueue( this_pdu );
	/** And return that we are done with the previous PDU. */
	return STATE_MACHINE_ID_INVALID;
    }

    /** If the server is shutting down, we do not accept anymore of these requests... */
    if( adarendezvous_shutdown ) {
	warning( "ADArendez-vous: dropping incoming request because of server shutdown.\n" );
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    }

    /** Read the header. */
    if( read_header( this_pdu ) < 0 ) {
	/** Something is wrong with the ADA header. */
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    }
#if 0 
    /** I am not sure anymore what this part does... */
    /** Lower layers error handling. */
    if( this_pdu->errno != EPDU_SUCCESS ) {
	/** Error notification. */
	/** Create a backup ID using the generation and sequence IDs. */
	snprintf( backup_key, sizeof(backup_key), ADA_PREFIX "%d.%d", this_pdu->genid, this_pdu->seqid );
	/** Destroy the backup. */
	backup_cancel( backup_key );
	/** Forward up the stack the PDU. */
	return imago_layer_marshaling_up_stateid;
    }
#endif
    /** We have extracted the ADA header, Now, we can just backup the PDU, and send a 
	"accept?" control PDU to the other side.
	If this PDU has been received already (Accept? packet lost), 
	then we do it all over again. Not a big deal... */
    snprintf( backup_key, sizeof(backup_key), ADA_PREFIX "%s:%d.%d", 
	      this_pdu->source_server, this_pdu->genid, this_pdu->seqid );
    /** Allocate a timer BEFORE the PDU is backup. */
    this_pdu->timer = timer_new();
    if( (b_id = backup_save(backup_key, this_pdu)) < 0 ) {
	/** Could not backup the PDU... :( */
	error( "Unable to backup incoming PDU.\n", backup_key );
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    }
    /** Dispatch a "accept" packet. */
    dispatch_control_pdu( ADA_TYPE_ACCEPT, this_pdu );
    /** No matter if we have been able to dispatch the response
	we still keep the backup in storage. 
	If the dispatch packet did not go throught, the peer
	will eventually retry.
    */
    /** Start the timer. */
    timer_set( this_pdu->timer, &tv_rcv, timeout_callback, (void*)b_id );
    /** The old PDU is not required anymore... Destroy it. */
    pdu_free( this_pdu );
    return STATE_MACHINE_ID_INVALID;
}

state_id_t imago_layer_ADArendezvous_up( register struct pdu_str *this_pdu ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
    if( this_pdu->flags & PDU_TYPE_SITP )
	return imago_layer_ADArendezvous_transport_up( this_pdu );
    else
	return imago_layer_ADArendezvous_control_up( this_pdu );
}




