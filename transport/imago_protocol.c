/** $RCSfile: imago_protocol.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_protocol.c,v 1.27 2003/03/24 14:07:48 gautran Exp $ 
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
#include <signal.h>
#include <sched.h>
#include <string.h>
#include <stdlib.h>

#include "include.h"
#include "timer.h"
#include "imago_icb.h"
#include "imago_pdu.h"
#include "imago_signal.h"
#include "state_machine.h"
#include "frame.h"
#include "imago_layer_connection.h"
#include "imago_layer_routing.h"
#ifndef _ROUTER_ONLY 
#include "imago_layer_security.h"
#include "imago_layer_ADArendezvous.h"
#include "imago_layer_marshaling.h"
#endif
#include "imago_protocol.h"


/** Terminate the program. */
int imago_server_stop = 0;

#ifdef _REENTRANT
/** Used to protect and lock the pthread_count variable. */
pthread_mutex_t pthread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  pthread_count_cond = PTHREAD_COND_INITIALIZER;	
int pthread_count = 0;
int pthread_waiting = 0;
int protocol_max_thread = IMAGO_PROTOCOL_MAX_THREAD;

/** Used to protect and lock the protocol_recv_handler variable. */
pthread_mutex_t imago_protocol_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
/** In a multi-threaded (aka: asynchronous) application, receiving ICB's is done
    through registration of one single handler. */
transport_recv_t protocol_recv_handler = NULL;
const int SIGrestart = SIGUSR2;


void(*imago_layer_init_table[])( ) = {
    imago_signal_init,              /** Initialize the signals. */
    timer_init,                     /** Initialize the timer. */
    frame_init,                     /** Initialize the frames. */
    imago_layer_connection_init,    /** This is the lowest layer. P-to-P Connection layer. */
    imago_layer_routing_init,       /** This is Routing layer. S-to-D. */
#ifndef _ROUTER_ONLY 
    imago_layer_security_init,      /** This is security layer. */
    imago_layer_ADArendezvous_init, /** This is ADArendezvous layer. */
    imago_layer_marshaling_init,    /** This is the marshaling layer. */
#endif
    /** MUST be NULL terminated. */
    NULL
};

void(*imago_layer_end_table[])( ) = {
    imago_layer_connection_terminate,    /** This is the lowest layer. P-to-P Connection layer. */
    imago_layer_routing_cleanup,         /** Clean up the routing layer. */
    frame_cleanup,                       /** Clean up all unsused frames. */
    state_machine_cleanup,               /** Cleaning up the state machine table. */
    /** MUST be NULL terminated. */
    NULL
};

void(*imago_layer_shutdown_table[])( ) = {
    imago_layer_connection_shutdown,     /** This is the lowest layer. P-to-P Connection layer. */
#ifndef _ROUTER_ONLY 
    imago_layer_ADArendezvous_shutdown,  /** This is ADA rendezvous layer. */
#endif
    /** MUST be NULL terminated. */
    NULL
};

/** State machine ids. */
state_id_t imago_protocol_up_stateid = -1;
state_id_t imago_protocol_down_stateid = -1;
/** */

state_id_t imago_protocol_up( struct pdu_str *);
state_id_t imago_protocol_down( struct pdu_str *);


void transport_shutdown_aux( int wait ) {
    int i;
    transport_signal();
    /** Shutdown all layers. */
    for( i = 0; imago_layer_shutdown_table[i] != NULL; i++ ) 
	imago_layer_shutdown_table[i]();

#ifdef _REENTRANT
    if( wait ) {
	/** Before we wait for everyone, signal everyone... */
	pthread_mutex_lock(&pthread_count_mutex);
	while( pthread_count != 0 )
	    pthread_cond_wait( &pthread_count_cond, &pthread_count_mutex ); 
	pthread_mutex_unlock(&pthread_count_mutex);
    }
#endif
}


/** Cleanly shutdown the transport protocol. */
void transport_shutdown() {
    transport_shutdown_aux(0);
}
/** This allows the outside world to brutally terminate the transport protocol stack. */
void transport_terminate() {
    int i;
    imago_server_stop = 1;
    /** Shut down and wait all transport threads. */
    transport_shutdown_aux(1);
    /** Terminate all layers one by one. */
    for( i = 0; imago_layer_end_table[i]; i++ ) {
	imago_layer_end_table[i]();
    }
}

void transport_signal() {
    int wc = 0;
#ifdef _REENTRANT
    pthread_mutex_lock( &imago_protocol_mutex );
    wc = pthread_waiting;
    pthread_mutex_unlock( &imago_protocol_mutex );
#endif
    if( wc ) {
	if( imago_signal_kill(SIGrestart) < 0 ) {
	    info( "Threads signaling too fast...\n" );
	}
    }
}
void transport_signal_wait() {
#ifdef _REENTRANT
    pthread_mutex_lock( &imago_protocol_mutex );
    pthread_waiting++;
    pthread_mutex_unlock( &imago_protocol_mutex );
#endif
    imago_signal_wait();
#ifdef _REENTRANT
    pthread_mutex_lock( &imago_protocol_mutex );
    pthread_waiting--;
    pthread_mutex_unlock( &imago_protocol_mutex );
#endif

}
int transport_run() {
    struct pdu_str *this_pdu = NULL;
    /** Once this is all set, process the eventual imago_pdu. */ 
    if( (this_pdu = pdu_dequeue()) != NULL ) {
	/** We safely grabed one PDU. */
	/** Call up the state machine on this PDU. */
	if( state_machine( &this_pdu->state, this_pdu ) > 0 ) {
	    /** The new state is greater than zero so it means the PDU is still valide. 
		Put it back into the queue. */
	    pdu_enqueue( this_pdu );
	}
	/** Yield for a better process repartition. */
	//sched_yield();
	return 1;
    }
    /** Return zero to indicate no PDU has been handled. */
    return 0;
}
int transport_run_signals() {
    return imago_signal_poll();
}


void SIGTERM_handler( int sig, void *arg ) {
    debug( "%s(%d)\n", __PRETTY_FUNCTION__, arg ); 
    transport_shutdown();
}
void SIGINT_handler( int sig, void *arg ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, arg ); 
    transport_terminate();
}
void SIGHUP_handler( int sig, void *arg ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, arg ); 
}
void SIGrestart_handler( int sig, void *arg ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, arg ); 
}

void transport_init() {
    int i;

    /** Initialize all layers one by one. */
    for( i = 0; imago_layer_init_table[i]; i++ ) {
	imago_layer_init_table[i]();
    }
    /** */
    /** Set the signal handler for the SIGTERM. */
    imago_signal( SIGTERM, SIGTERM_handler, NULL );
    /** Set the signal handler for the SIGINT. */
    imago_signal( SIGINT, SIGINT_handler, NULL );
    /** Set the signal handler for the SIGHUP. */
    imago_signal( SIGHUP, SIGHUP_handler, NULL );
    /** Set the signal handler for the SIGUSR2. */
    imago_signal( SIGrestart, SIGrestart_handler, NULL );
    /** */
#ifndef _ROUTER_ONLY 
    /** Register the states in the state machine. */
    imago_protocol_up_stateid = state_machine_register( (state_action_t)imago_protocol_up );
    imago_protocol_down_stateid = state_machine_register( (state_action_t)imago_protocol_down );
    /** */
#endif
}

int imago_protocol( ) {
    return transport_run_signals() + transport_run();
}

/** In a multi-threaded (aka: asynchronous) application, receiving ICB's is done
    through registration of one single handler. */
transport_recv_t transport_register_callback( transport_recv_t new_handler ) {
    transport_recv_t old_handler;
#ifdef _REENTRANT
    pthread_mutex_lock( &imago_protocol_mutex );
#endif
    old_handler = protocol_recv_handler;
    protocol_recv_handler = new_handler;
#ifdef _REENTRANT
    pthread_mutex_unlock( &imago_protocol_mutex );
#endif
    return old_handler;
}

/** Used to set/get options in the transport stack. */
int get_transportopt( int layer, int opt, void *arg ) {
    switch( layer ) {
    case LAYER_TRANSPORT:
	switch( opt ) {
	case T_MAX_THREAD:
#ifdef _REENTRANT
	    *(int*)arg = protocol_max_thread;
#else
	    *(int*)arg = 0;
#endif
	    break;
	default:
	    return -1;
	}
	break;
    case LAYER_CONNECTION:
	return imago_layer_connection_getopt( opt, arg );
    case LAYER_ROUTING:
	return imago_layer_routing_getopt( opt, arg );
#ifndef _ROUTER_ONLY
    case LAYER_SECURITY:
	return imago_layer_security_getopt( opt, arg );
    case LAYER_ADARENDEZVOUS:
	return imago_layer_ADArendezvous_getopt( opt, arg );
    case LAYER_MARSHALING:
	return imago_layer_marshaling_getopt( opt, arg );
#endif
    default:
	return -1;
    }
    return 0;
}
int set_transportopt( int layer, int opt, void *arg ) {
    switch( layer ) {
    case LAYER_TRANSPORT:
	switch( opt ) {
	case T_MAX_THREAD:
#ifdef _REENTRANT
	    protocol_max_thread = *(int*)arg;
#endif
	    break;
	default:
	    return -1;
	}
	break;
    case LAYER_CONNECTION:
	return imago_layer_connection_setopt( opt, arg );
    case LAYER_ROUTING:
	return imago_layer_routing_setopt( opt, arg );
#ifndef _ROUTER_ONLY
    case LAYER_SECURITY:
	return imago_layer_security_setopt( opt, arg );
    case LAYER_ADARENDEZVOUS:
	return imago_layer_ADArendezvous_setopt( opt, arg );
    case LAYER_MARSHALING:
	return imago_layer_marshaling_setopt( opt, arg );
#endif
    default:
	return -1;
    }
    return 0;
}
/** */

#ifdef _REENTRANT
void *imago_protocol_thread( void *io_master )
#else
     void imago_protocol_thread( void ) 
#endif
{
    unsigned int counter = 0;
#ifdef _REENTRANT
    pthread_mutex_lock(&pthread_count_mutex);
    pthread_count++;
    pthread_mutex_unlock(&pthread_count_mutex);
#endif
    /** As long as we are not asked to stop, process the entries. */
    while( 1 ) {
	/** Call up the main entry function for the imago protocol. */
	if( imago_protocol() ) {
	    counter++;
	} else {
	    /** No PDU to handle. Wait until something is to be done. */
#ifdef _REENTRANT
	    debug( "transport thread [%ld]: counter=%d\n",  pthread_self(), counter );
#else
	    debug( "transport thread: counter=%d\n",  counter );
#endif
	    /** If there is nothing to do and we received the order to shutdown or terminate,
		break out of this loop. */
	    if( imago_server_stop ) break;
	    /** Otherwise, wait until something needs to be down. */
	    imago_signal_wait();
	}
    }
#ifdef _REENTRANT
    /** Signal the main thread that we past out... */
    pthread_mutex_lock(&pthread_count_mutex);
    pthread_count--;
    if( pthread_count == 0 )
	pthread_cond_signal( &pthread_count_cond ); 
    pthread_mutex_unlock(&pthread_count_mutex);
    /** */
    return NULL;
#endif
}

int transport_spawn() {
    /** This is the main function for the process control. */
#ifdef _REENTRANT
    {
	int i = 0;
	pthread_t tid;
	pthread_attr_t attr;
	sigset_t sigs_to_block;
	/** Initialize the newly created thread attribute. */
	pthread_attr_init( &attr );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

	/** Block all signals */
	sigfillset( &sigs_to_block );
	pthread_sigmask( SIG_BLOCK, &sigs_to_block, NULL );

	for( i = 0; i < protocol_max_thread; i++ ) 
	    pthread_create( &tid, &attr, imago_protocol_thread, NULL );

	/** Don't need this anymore... */
	pthread_attr_destroy( &attr );
    }
#else
    /** Multi-threaded application is not supported. */
    return -1;
#endif
    return 0;
}


#ifndef _ROUTER_ONLY 
state_id_t imago_protocol_down( struct pdu_str *this_pdu ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
    /** Make sure there is an ICB to send. We do not support sending anything else. */
    if( this_pdu->icb_ptr == NULL ) {
	warning( "%s: Invalid ICB (request ignore)", __PRETTY_FUNCTION__ );
	/** Then, clean up the PDU. */
	pdu_free( this_pdu );
	/** And return an invalid state. */
	return STATE_MACHINE_ID_INVALID;
    }
    return imago_layer_marshaling_down_stateid;
}

state_id_t imago_protocol_up( struct pdu_str *this_pdu ) {
    transport_recv_t cb_fcntl = NULL;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
#ifdef _REENTRANT
    pthread_mutex_lock( &imago_protocol_mutex );
#endif
    cb_fcntl = protocol_recv_handler;
#ifdef _REENTRANT
    pthread_mutex_unlock( &imago_protocol_mutex );
#endif
    /** If a receive handler has be registered, use it. */
    if( cb_fcntl ) cb_fcntl( this_pdu->source_server, this_pdu->icb_ptr );
    /** Then, clean up the PDU. */
    pdu_free( this_pdu );
    /** And return an invalid state. */
    return STATE_MACHINE_ID_INVALID;
}

/** Synchronous and asynchronous send of ICB's. */
int transport_send ( const char *to, struct imago_control_block *this_icb ) {
    struct pdu_str *this_pdu = NULL;
    /** First of all, make sure this input is not missing. */
    if( !this_icb ) 
	return -1;

    /** Then allocate a new imago_pdu. */
    if( (this_pdu = pdu_new()) == NULL ) {
	/** This is a shortage of PDU's. */
	/**
	 * TO DO: Deal with this case by providing a wait.
	 */
	return -1; /** For now. */
    }
    /** Initialize the PDU. */
    /** First the state. */
    pdu_set_state( this_pdu, imago_protocol_down_stateid );
    /** Then the marshaling data. */
    this_pdu->icb_ptr = this_icb;
    /** And finaly, where to send the ICB to. */
    strncpy( this_pdu->destination_server, to, MAX_DNS_NAME_SIZE-1 );
    /** Once this is all set, push the pdu into the IO queue to be sent. */
    pdu_enqueue( this_pdu );
#ifndef _REENTRANT
    /** In the case this is *not* a multi-threaded application, then, synchronously send the pdu. */
    while( imago_protocol() );
#endif
    /** Otherwise and once the pdu has been sent, return. */
    return 0;
}

#ifndef _REENTRANT
struct imago_control_block *imago_protocol_receive() {


    return NULL;
}
#endif

#endif //#ifndef _ROUTER_ONLY 

/** STRING HASH FUNCTION. */
int strhash_np(const char* key, const int size) {
	unsigned int h, v;
	int i;
	for (h = 0, i = 1, v = 0; *key; i++, key++) {
	    v = ((v << 8) | *key);
	    if( !(i % sizeof(int)) ) h = h + (v * i );
	}
	h = h + (v * i );
	if ((int) h >= 0) return (int) (h % size);
	else return (-(int)h) % size;
}
#if 0
int strhash_np(const char* key, const int size) {
	unsigned int h;
	for (h = 0; *key; key++) h = ((h * h) << 2) + *key;
	if ((int) h >= 0) return (int) (h % size);
	else return (-(int)h) % size;
}
#endif
/** */
