/** $RCSfile: imago_signal.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_signal.c,v 1.13 2003/03/21 13:12:03 gautran Exp $ 
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
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <signal.h>
#include <sched.h>


#include "include.h"
#include "imago_signal.h"

struct  sig_entry {
    void (*cb)( int, void * );
    void *arg;
};

struct sig_entry imago_signal_table[_NSIG];

#ifdef _REENTRANT
/** The POSIX thread implementation in the Linux kernel prior to 2.4.20 and libpthread prior to 0.10
    handle threads as LWP. The signals are delivered to the LWP it targets. 
    Because of this, it is not possible to dedicate one thread as signal handler unless you make sure that 
    all signals are directed to this particular thread.
    Instead, we use all thread to receive and handle any signal. The down side is that signal handler may be
    run many times if a signal is sent to the process (every thread) instead of one particular thread. */
static pthread_key_t sig_key;

#else
static int imago_sig = 0;
#endif

#ifndef _REENTRANT
void null_handler( register int sig ) {
    imago_sig = sig;
}
#else
void null_handler( int sig ) {
}
#endif


#ifdef _REENTRANT
void imago_signal_async_wait() {
    sigset_t sigs_to_catch, *sig_val;
    int caught, i;
    sigemptyset( &sigs_to_catch );
    for( i = 0; i < _NSIG; i++ ) {
	if( imago_signal_table[i].cb != NULL ) {
	    sigaddset( &sigs_to_catch, i );
	}
    }
    sigwait( &sigs_to_catch, &caught );
    debug( "pthread[%ld]: caught = %d\n",  pthread_self(), caught );
    /** Remember on a per thread bases what signal has been received. */
    if( (sig_val = pthread_getspecific( sig_key )) == NULL ) {
	sig_val = malloc( sizeof(sigset_t) );
	sigemptyset( sig_val );
    }
    sigaddset( sig_val, caught );
    pthread_setspecific( sig_key, sig_val );
}

void signal_destroy( void *sig_val ) {
    free( sig_val );
}
#endif


void imago_signal_init() {
    sigset_t sigs_to_block;
    /** Reset the safe signal handler table. */
    memset( imago_signal_table, 0, sizeof(imago_signal_table) );
    /** Block all signals */
    sigfillset( &sigs_to_block );
#ifdef _REENTRANT
    /** Create the per thread data key used to store the signal number. */
    pthread_key_create( &sig_key, signal_destroy );
    pthread_sigmask( SIG_BLOCK, &sigs_to_block, NULL );
#endif
}
inline int sigisempty_np( register sigset_t *sig ) {
    register int i;
    for( i = 0; i < _NSIG; i++ ) {
	if( sigismember(sig, i) > 0) {
	    return 1;
	}
    }
    return 0;
}

int imago_signal_read_reset() {
    int sig = 0;
#ifdef _REENTRANT
    register int i;
    register sigset_t *sig_val;
    if( (sig_val = pthread_getspecific( sig_key )) == NULL ) {
	sig_val = malloc( sizeof(sigset_t) );
	sigemptyset( sig_val );
	sigpending( sig_val );
    } 
    /** May be we had no time to go into the wait for signal function
	in that case, we need to check the pending signals to see if there is anything. */
    if( !sigisempty_np(sig_val) ) {
	sigpending( sig_val );	
	/** Once we have looked at those signals, we need to clear them from the
	    pending queue. */
	if( sigisempty_np(sig_val) ) {
	    sigwait( sig_val, &sig );
	}
    }
   
    for( i = 0; i < _NSIG; i++ ) {
	if( sigismember(sig_val, i) > 0) {
	    sigdelset( sig_val, i );
	    sig = i;
	    break;
	}
    }
    pthread_setspecific( sig_key, sig_val );
#else
    sig = imago_sig;
    imago_sig = 0;
#endif
    return sig;
}

void imago_signal( int sig,  void (*handler)(int, void *), void *arg ) {
    struct sigaction saction;
    debug( "%s:(%d, %p)\n", __PRETTY_FUNCTION__, sig, handler );
    /** Be sure the signal is within boundaries. */
    if( sig < 0 || sig >= _NSIG ) return;
    /** Clean up the sigaction structure. */
    memset( &saction, 0, sizeof( struct sigaction ) );
    /** Set the signal handler for the specific signal. */
    imago_signal_table[sig].cb = handler;
    imago_signal_table[sig].arg = arg;
    /** Set our dummy handler. */
    saction.sa_handler = null_handler;
    saction.sa_flags = SA_RESTART;
    sigemptyset( &saction.sa_mask );
    sigaddset( &saction.sa_mask, sig );
    /** There is a LWP issue here... If the threads are created already, setting a signal this 
	way does not work. The signals will not be received (queued pending)
	and will be processed only after one signal (initialized before pthread_create)
	is received. The reason is that will waiting for a signal, the threads cannot update their
	Signal mask and then block it. */
    /** Set the process signal handler mask to deliver this signal. */
    sigaction( sig, &saction, NULL );
    sigprocmask( SIG_BLOCK, &saction.sa_mask, NULL );
}

void imago_signal_safe_handler( int sig ) {
    debug( "%s:(%d)\n", __PRETTY_FUNCTION__, sig );
    /** Be sure the signal is within boundaries. */
    if( sig < 0 || sig >= _NSIG ) return;
    /** Dispatch the signal to the appropriate handler. */
    if( imago_signal_table[sig].cb )
	imago_signal_table[sig].cb( sig, imago_signal_table[sig].arg );
}

int  imago_signal_poll() {
    register int sig;
    register int count = 0;
    for( count = 0; (sig = imago_signal_read_reset()) != 0; count++ )
	imago_signal_safe_handler( sig );

    return count;
}

int imago_signal_kill( int sig ) {
    return killpg(0, sig);
}
