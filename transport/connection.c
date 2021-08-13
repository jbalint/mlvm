/** $RCSfile: connection.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: connection.c,v 1.3 2003/03/21 13:12:03 gautran Exp $ 
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
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "include.h"
#include "frame.h"
#include "timer.h"
#include "connection.h"
#include "imago_pdu.h"

/** The connection keeper tools. */
int con_timeout_sec = CONNECTION_TIMEOUT_SEC;
/** Every now and then, all connections are checked, old ones are closed, 
    as well as invalid ones. */
timer_id timer = -1;
struct timeval tv_c = {
    tv_sec: CLEANUP_PERIOD,
    tv_usec: 0,
};
#ifdef _REENTRANT
pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
void SIGALRM_cleanup( timer_id, void * );
/** */
static con_t hash_table[IMAGO_MAX_CONNECTION];
con_t *hash_new( u_int32_t );
con_t *hash( u_int32_t, int );


/** 
 * con_init() - Initializer routine for connection module.
 *              The return value indicate is the initialization went fine or not. 
 */
int con_init() {
    /** First, clear the hash table. */
    memset( hash_table, 0, sizeof(hash_table) );
    /** Initialize the Connection Timer. */
    timer = timer_new();
    /** Set the timer to fire in CONNECTION_TIMEOUT second. */
    timer_set( timer, &tv_c, SIGALRM_cleanup, NULL );
#ifdef _REENTRANT  
    {
	int i;
	/** Then, go through all entry and initialize all mutexes and cond variables. */
	for( i = 0; i < IMAGO_MAX_CONNECTION; i++ ) {
	    pthread_mutex_init( &hash_table[i].lock, NULL );
	    pthread_cond_init( &hash_table[i].l_cv, NULL );
	}
    }
#endif
    return 1;
}
/**
 * con_end() - Terminate cleanly all open connection, free all memory and clear it all.
 *             This function must be called last and only a call to con_init() may 
 *             reactivate the connection module.
 */
void con_end() {
    con_t *c_h;
    debug( "%s()\n", __PRETTY_FUNCTION__); 

    /** Scan through the list of active connection to read the input packet. */
    for( c_h = con_next(NULL); c_h; c_h = con_next(c_h) ) {
	/** Check if there is anthing to do with this connection. */
	if( con_lock( c_h ) == 0 ) { /** Try locking the connection */
	    /** This connection is way too old. */
	    con_free( c_h ); 
	}
    }

#ifdef _REENTRANT
    /** One thread in this at a time is more than enough... */
    pthread_mutex_lock( &timer_mutex );
#endif
    /** Reset the timer so it can fire again soon. */
    timer_free( timer );
#ifdef _REENTRANT
    /** One thread in this at a time is more than enough... */
    pthread_mutex_unlock( &timer_mutex );
#endif
}
/**
 * con_trylock(..)  -  try to lock the connection structure.. 
 *                         return  0, if it succeed
 *                         return -1, if it fails
 *                      If succeed the connection is locked.
 *                       DO NOT WAIT ! ! !
 */
int con_trylock( con_t *c_h ) {
    int ret= 0;
#ifdef _REENTRANT
    pthread_mutex_lock( &c_h->lock ); 
#endif
    if( c_h->status == C_ACTIVE ) {
	c_h->status = C_LOCKED;
	ret = 0;
    } else {
	ret = -1;
    }
#ifdef _REENTRANT
    pthread_mutex_unlock( &c_h->lock ); 
#endif
    return ret;
}
/**
 * con_lock(..)  -  lock the connection structure.
 *                       return  0, if it succeed
 *                       return -1, if it fails
 *                   If succeed, the connection is locked
 *                   ! ! ! WAIT ! ! !
 */
int con_lock( con_t *c_h  ) {
    int ret= 0;
#ifdef _REENTRANT
    pthread_mutex_lock( &c_h->lock ); 
 repeat:
#endif
    if( c_h->status == C_ACTIVE ) {
	c_h->status = C_LOCKED;
	ret = 0;
#ifdef _REENTRANT
    } else if( c_h->status == C_LOCKED ) {
	pthread_cond_wait( &c_h->l_cv, &c_h->lock );
	goto repeat;
#endif
    } else {
	ret = -1;
    }
#ifdef _REENTRANT
    pthread_mutex_unlock( &c_h->lock ); 
#endif
    return ret;
}

/**
 * con_unlock(..)  -  try to unlock the connection structure. 
 *                        return  0, if it succeed
 *                        return -1, if it fails
 *                     If succeed, the connection is unlock, and a signal is sent
 *                     to unblock waiters for this particular connaction.
 */
int con_unlock( con_t *c_h ) {
    int ret= 0;
#ifdef _REENTRANT
    pthread_mutex_lock( &c_h->lock ); 
#endif
    if( c_h->status == C_LOCKED ) {
	c_h->status = C_ACTIVE;
	ret = 0;
    } else {
	ret = -1;
    }
#ifdef _REENTRANT
    if( !ret ) pthread_cond_signal( &c_h->l_cv );
    pthread_mutex_unlock( &c_h->lock ); 
#endif
    return ret;
}

/**
 *  con_new(...)  -  Create a new, initialized but not connected connection.
 */
con_t *con_new(struct sockaddr_in *raddr) {
    con_t *c_h = (con_t *)hash(raddr->sin_addr.s_addr, C_FREE);
    if( c_h == NULL ) {
	warning( "Too many active connections.\n" );
	return NULL;
    }
    /** Copy the destionation. */
    memcpy( &c_h->peer, raddr, sizeof(c_h->peer) );
    /** Clear the local data. */
    memset( &c_h->local, 0, sizeof(c_h->local) );
    /** If this is an active connection, lock it while we process it. */
    c_h->status = C_LOCKED;
#ifdef _REENTRANT
    pthread_mutex_unlock( &c_h->lock ); 
#endif
    /** Everything else must be in a valid state. */
    return c_h;
}
/**
 *   con_free(...) - Close and free all resources used by this connection. 
 */
void con_free( con_t *c_h ) {
    debug( "%s(%d)\n", __PRETTY_FUNCTION__, c_h ); 
    /** Check if it is a locked connection. */
    if( con_trylock(c_h) >= 0 ) {
	/** We cannot free an unlocked connection. */
	con_unlock( c_h );
	return;
    }
    /** Send the log. */
    notice( "Closing connection: [%s:%d].\n", 
	    inet_ntoa(c_h->peer.sin_addr), 
	    ntohs(c_h->peer.sin_port) );
    /** Shutdown the socket. */
    shutdown( c_h->fd, SHUT_RDWR );
    /** In between, clear all the frames. */
    frame_destroy( &c_h->frames );
    /** Then, close the socket. */
    close( c_h->fd );
    /** We are all set. Release this entry. */
#ifdef _REENTRANT
    pthread_mutex_lock( &c_h->lock ); 
#endif
    /** If this is an active connection, lock it while we process it. */
    c_h->status = C_FREE;
#ifdef _REENTRANT
    pthread_cond_signal( &c_h->l_cv );
    pthread_mutex_unlock( &c_h->lock ); 
#endif
}
/**
 *  lookup_con( ... )  -  Lookup a specific destination in the connection table. 
 *                        return its handler if found, NULL otherwise.
 */
con_t *lookup_con( struct sockaddr_in *raddr ) {
    con_t *c_h = hash(raddr->sin_addr.s_addr, C_ACTIVE);
#ifdef _REENTRANT
    if( c_h ) 
	pthread_mutex_unlock( &c_h->lock );
#endif
    if( c_h ) {
	if( con_trylock( c_h ) < 0 ) {
	    c_h = NULL; /** In use... */
	}
    }
    return c_h;
}

/**
 *  make_con( ... )  -  Connect this connection handle to some remote address. 
 *                      return the connection handler if succeed, NULL otherwise.
 *                      If the connection is already connected, the previous 
 *                      connection is terminated, all resources are freed,
 *                      Then, the same structure is used for the new address.
 *                      If the c_h is NULL, a new connection handle is allocated
 *                      and returned.
 */

con_t *make_con( con_t *c_h, struct sockaddr_in *raddr, pid_t io_master ) {
    int s; 
    if( !c_h ) 
	c_h = con_new(raddr);
    if( !c_h )
	return NULL;

    /** Now open the connection (TCP). */
    if( (c_h->fd = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
	/** We could not open the socket. */
	warning( "Unable to create a new socket.\n" );
	con_free( c_h );
	return NULL;
    }
    /** And connect it to the remote peer. */
    if( connect(c_h->fd, (struct sockaddr *)raddr, sizeof(struct sockaddr_in)) < 0 ) {
	/** We could not connect the socket. */
	warning( "Unable to connect to host [%s].\n", inet_ntoa(raddr->sin_addr) );
	con_free( c_h );
	return NULL;
    }
    /** Update the local information of this socket. */
    s = sizeof(struct sockaddr_in);
    getsockname( c_h->fd, &c_h->local, &s);
    /** Set the descriptor to send signals. */
    fcntl( c_h->fd, F_SETSIG, SIGIO );
    fcntl( c_h->fd, F_SETFL, O_ASYNC | O_NONBLOCK );
    fcntl( c_h->fd, F_SETOWN, io_master );
    /** */
    return c_h;
}

/**
 *  con_rcv(...) - Receive any data pending on this connection. 
 *                 The function DO NOT BLOCK in any case.
 *                 The return value is the amount of byte received.
 */
int con_rcv( con_t *c_h ) {
    int err = 0, amount = 0;
    /** Test for the amount of data present in this socket buffer. */
    err = ioctl( c_h->fd, FIONREAD, &amount );
    if( err >= 0 && amount > 0 ) {
	char buf[amount]; /** So we can read the whole thing at once... */
	/** We seem to have something to read. */
	if( (amount = recv( c_h->fd, buf, amount, 0)) > 0) {
	    /** Add it to the frames in the correct order. */
	    amount = frame_append_string( &c_h->frames, buf, amount ); 
	}
    }
    return amount;
}

/**
 * SIGALRM_cleanup(...) - Call every CLEANUP_PERIOD by the timer to clean and flush old connections.
 */
void SIGALRM_cleanup( timer_id id, void *arg ) {
    time_t timestamp = 0;
    con_t *c_h;
    debug( "%s(%d, %p)\n", __PRETTY_FUNCTION__, id, arg ); 

    /** Get the current time. */
    timestamp = time(NULL) - con_timeout_sec;
    
    /** Scan through the list of active connection to read the input packet. */
    for( c_h = con_next(NULL); c_h; c_h = con_next(c_h) ) {
	/** Check if there is anthing to do with this connection. */
	if( con_trylock( c_h ) == 0 ) { /** Try locking the connection */
	    if( (c_h->timestamp < timestamp) || (con_test(c_h) < 0) ) {
		/** This connection is way too old. */
		con_free( c_h ); 
	    } else {
		con_unlock( c_h );
	    }
	}
    }
    /** All done. */
#ifdef _REENTRANT
    /** One thread in this at a time is more than enough... */
    pthread_mutex_lock( &timer_mutex );
#endif
    /** Reset the timer so it can fire again soon. */
    timer_set( id, &tv_c, SIGALRM_cleanup, NULL );
#ifdef _REENTRANT
    /** One thread in this at a time is more than enough... */
    pthread_mutex_unlock( &timer_mutex );
#endif
}


/**
 * con_test(...) - Test weither or not the connection is still open on the other side. 
 *                 Return 1 if test passed, -1 id failed and 0 if nothing had been done (arg == NULL).
 */
inline int con_test( con_t *c_h ) {
    char c;
    if( c_h == NULL ) return 0;
    if( recv(c_h->fd, &c, 1, MSG_PEEK | MSG_DONTWAIT) == 0 ) {
	/** Test failed. */
	return -1;
    }
    /** Test passed. */
    return 1;
}

/**
 * con_next(...) - Gets the valid connection at the next position.
 */
con_t *con_next( con_t *c_h ) {
    c_h = (c_h == NULL)? &hash_table[0]: c_h + 1;
    
    for( ; c_h <= &hash_table[IMAGO_MAX_CONNECTION-1]; c_h++ ) {
#ifdef _REENTRANT
	pthread_mutex_lock( &c_h->lock );
#endif
	if( c_h->status != C_FREE ) {
#ifdef _REENTRANT
	    pthread_mutex_unlock( &c_h->lock );
#endif
	    return c_h;
	}
#ifdef _REENTRANT
    pthread_mutex_unlock( &c_h->lock );
#endif
    }
    return NULL;
}
/**
 * hash(...) - Hash a key and return the corresponding avaible slot pointer.
 */
con_t *hash( u_int32_t key, int status ) {
    int slot = key % IMAGO_MAX_CONNECTION;
    con_t *c_h = &hash_table[slot];
    /** If the slot is empty, return NULL. */
    while(1) {
#ifdef _REENTRANT
	pthread_mutex_lock( &c_h->lock );
#endif
	if( c_h->status == C_FREE) {
	    /** Get off in any case... */
	    if( status != C_FREE ) {
#ifdef _REENTRANT
		pthread_mutex_unlock( &c_h->lock );
#endif
		c_h = NULL;
	    }
	    break;
	}
	if( (c_h->status == status) && (c_h->peer.sin_addr.s_addr == key) ) {
	    break;
	}
#ifdef _REENTRANT
	pthread_mutex_unlock( &c_h->lock );
#endif
	c_h = (c_h == &hash_table[IMAGO_MAX_CONNECTION-1]) ? &hash_table[0]: c_h + 1;
	if( c_h == &hash_table[slot] ) {
	    c_h = NULL;
	    break;
	}
    }
    return c_h;
}

/**
 * hash_new(...) - Hash a key and return the corresponding empty slot pointer.
 */
con_t *hash_new( u_int32_t key ) {
    int slot = key % IMAGO_MAX_CONNECTION;
    con_t *c_h = &hash_table[slot];
    /** If the slot is empty, return NULL. */
    while(1) {
#ifdef _REENTRANT
	pthread_mutex_lock( &c_h->lock );
#endif
	if( c_h->status == C_FREE ) 
	    break;
#ifdef _REENTRANT
	pthread_mutex_unlock( &c_h->lock );
#endif
	c_h = (c_h == &hash_table[IMAGO_MAX_CONNECTION-1]) ? &hash_table[0]: c_h + 1;
	if( c_h == &hash_table[slot] ) {
	    c_h = NULL;
	    break;
	}
    }
    return c_h;
}
