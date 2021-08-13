/** $RCSfile: imago_layer_connection.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_connection.c,v 1.21 2003/03/24 14:07:48 gautran Exp $ 
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
/** #########################################################################
 *    Connection PDU header
 *  ######################################################################### 
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                          magic number                         |  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |Connection 
 * | Version Major | Version Minor |C|    flags    |   Reserved    |  |protocol
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |header
 * |                            length                             |  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                                                               | 
 * /                            data                               / 
 * |                                                               | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  
 *
 * Magic: 0x01234567
 * Bit6:  1 means transport pdu
 *        0 means control pdu
 *
 *  ######################################################################### */
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
#include "connection.h"
#include "state_machine.h"
#include "imago_pdu.h"
#include "imago_signal.h"
#include "imago_layer_routing.h"
#include "imago_layer_connection.h"
#include "timer.h"


/** Connection Options. */
struct Options {
    int imago_server_tcp_port;
    int imago_server_udp_port;
    int connection_timeout_sec;
};
static struct Options c_options = {
    imago_server_tcp_port: IMAGO_SERVER_PORT,
    imago_server_udp_port: IMAGO_SERVER_PORT,
    connection_timeout_sec: CONNECTION_TIMEOUT_SEC,
};


static struct link_head_str waiting_to_send;


struct connection_header {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t length;
};

/** State machine ids. */
state_id_t imago_layer_connection_up_stateid = -1;
state_id_t imago_layer_connection_down_stateid = -1;
state_id_t connection_async_send_stateid = -1;
/** */


/** The two server socket. */
#ifdef _REENTRANT
pthread_mutex_t stream_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static int stream_fd = -1;

#ifdef _REENTRANT
pthread_mutex_t dgram_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static int dgram_fd = -1;
static pid_t io_master = 0;
/** */


state_id_t imago_layer_connection_down( struct pdu_str * );
state_id_t imago_layer_connection_up( struct pdu_str * );
state_id_t connection_async_send( struct pdu_str * );


int read_udp_server_socket( void (*list_handler)( struct pdu_str * ) ) {
    int error, amount;
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    if( pthread_mutex_trylock( &dgram_fd_mutex ) == 0 ) { 
#endif
	if( dgram_fd >= 0 ) {
	    /** Retreive any new PDU */
	repeat:
	    /** Test for the amount of data present in this socket buffer. */
	    error = ioctl( dgram_fd, FIONREAD, &amount );
	    if( (error >= 0) && (amount > 0) ) {
		/** We have a PDU to read. */
		char buf[amount];
		/** Create a PDU to store the data in. */
		struct pdu_str *this_pdu = pdu_new();
		if( this_pdu != NULL ) {
		    int fromlen = sizeof( this_pdu->remote_addr );
		    amount = recvfrom( dgram_fd, buf, amount, 0, (struct sockaddr *)&this_pdu->remote_addr, &fromlen );
		    /** Store the data into the PDU. */
		    if( pdu_write_string( this_pdu, buf, amount ) < 0 ) {
			/** We just lost the pdu... */
			pdu_free( this_pdu );
		    } else {
			this_pdu->local_addr.sin_family = PF_INET;
			this_pdu->local_addr.sin_port = htons( c_options.imago_server_udp_port );
			this_pdu->local_addr.sin_addr.s_addr = INADDR_ANY;
			this_pdu->flags = PDU_FLAG_INCOMING;
			pdu_set_state( this_pdu, imago_layer_connection_up_stateid );

			/** If we got a new PDU, send it to the list. */
			if( list_handler )
			    list_handler( this_pdu );
			else
			    pdu_free( this_pdu );
			goto repeat;
		    }
		}
	    }
	}
#ifdef _REENTRANT
	pthread_mutex_unlock( &dgram_fd_mutex );
    } else {
	return 0;
    }
#endif
    return 1;
}



int read_tcp_server_socket( void (*list_handler)( struct pdu_str * ) ) {
    con_t *cid;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof( addr );
    int nfd;
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    if( pthread_mutex_trylock( &stream_fd_mutex ) == 0 ) { 
#endif
	if( stream_fd >= 0 ) {
	    /** Test if there are new connection to be accepted. */
	    while( (nfd = accept( stream_fd, &addr, &addrlen )) >= 0 ) {
		if( (cid = con_new(&addr)) == NULL ) {
		    /** We run out of connection slot. */
		    close(nfd);
		} else {
		    notice( "A new connection has been accepted: [%s:%d].\n", 
			    inet_ntoa(addr.sin_addr), ntohs(addr.sin_port) );
		    /** The new connection needs to be initialized. */
		    set_csocket( cid, nfd );
		    fcntl( nfd, F_SETSIG, SIGIO );
		    fcntl( nfd, F_SETFL, FASYNC | O_NONBLOCK );
		    fcntl( nfd, F_SETOWN, io_master );
		    con_update( cid );
		    con_unlock( cid );
		}
	    }
	}
#ifdef _REENTRANT
	pthread_mutex_unlock( &stream_fd_mutex );
    } else {
	return 0;
    }
#endif
    return 1;
}


/** This is the POSIX safe IO handler routine. */
void SIGIO_handler( int sig,  void *arg ) {
    con_t *cid = NULL;
    int amount = 0, err = 0;
    struct connection_header hdr;
    void (*list_handler)(struct pdu_str *) = arg;
    debug( "%s(%d, %p)\n", __PRETTY_FUNCTION__, sig, list_handler ); 

    /** Read the UDP server socket. */
    read_udp_server_socket( list_handler );

    /** Read the TCP server socket. */
    read_tcp_server_socket( list_handler );

    /** Scan through the list of active connection to read the input packet. */
    for( cid = con_next(NULL); cid; cid = con_next(cid) ) {
	/** Check if there is anthing to do with this connection. */
	if( con_trylock( cid ) == 0 ) { /** Try locking the connection */
	    if( (err = con_rcv(cid)) == 0 ) {
		/** Nothing has been received. */
		con_unlock( cid );
		continue;
	    } else if( err < 0 ) {
		/** If this is an error, the only thing to do is abort this connection... */
		con_free( cid );
		continue;
	    }
	    con_update( cid );
	    amount = get_avail( cid );
	repeat:
	    if( amount >= sizeof(struct connection_header) ) {
		/** Retrieve the connection header. */
		con_peek( cid, (char *)&hdr, sizeof(struct connection_header) );
		/** Check the magic number first. */
		hdr.magic = ntohl( hdr.magic );
		if( hdr.magic != MAGIC_NUMBER ) {
		    error("Bad Magic Number in packet.\n" );
		    con_free( cid );
		    continue;
		}
		/** Then, retreive the total amount of data in this pdu. */
		hdr.length = ntohl( hdr.length );
		/** substract the amount of data in the first frame. */
		amount -= (hdr.length + sizeof(struct connection_header));
		/** Do we have enough ?? */
		if( amount >= 0 ) { 
		    /** If we have enough, create a PDU, dump all data into it, and release it. */
		    struct pdu_str *this_pdu = pdu_new();
		    /** If we could not get a PDU, just ignore the all thing... */
		    if( this_pdu != NULL ) {
			/** We got a PDU, dump all information into it... */
			get_raddr( cid, &this_pdu->remote_addr );
			get_laddr( cid, &this_pdu->local_addr );
			this_pdu->flags = PDU_FLAG_INCOMING;
			pdu_set_state( this_pdu, imago_layer_connection_up_stateid );
			/** Copy the frames. */
			if( con_read(cid, &this_pdu->frames, hdr.length + sizeof(struct connection_header)) < 0 ) {
			    /** The frame Copy is pass/fail operation. If it fails, ignore the all thing and destroy that PDU. */
			    pdu_free( this_pdu );
			} else {
			    /** Enqueue the newly created PDU using the list handler if provided. */
			    if( list_handler )
				list_handler( this_pdu );
			    else
				pdu_free( this_pdu );
			    /** At this point, we did succeed to welcome an incoming PDU, may be we received two (or more)...
				So let us repeat the operation. */
			    goto repeat;
			}
		    }
		}
	    }
	    con_unlock( cid );
	}
    }
    /** May be this sigio indicate that some connection are ready to send more data. */
    if( size(&waiting_to_send) ) {
	struct pdu_str *this_pdu = (struct pdu_str *)dequeue(&waiting_to_send);
	for( ; this_pdu; this_pdu = (struct pdu_str *)dequeue(&waiting_to_send) ) {
	    list_handler(this_pdu);
	}
    }
}
/** */

/** This is the POSIX safe SIGPIPE handler routine. */
void SIGPIPE_handler( int sig,  void *arg ) { 
    con_t *cid = NULL;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, arg ); 
    /** Scan through the list of connection and test if there is anything to destroy. */
    for( cid = con_next(NULL); cid; cid = con_next(cid) ) {
	/** Check if it is an active connection. */
	if( con_trylock(cid) == 0 ) { /** Try locking the connection. */
	    /** Test this connection. */
	    if( con_test(cid) > 0 ) {
		/** Test is passed. */
		con_unlock( cid );
	    } else {
		/** test failed. */
		con_free( cid );
	    }
	}
    }
}
/** */


/** This is the server side of the application. It does not have to be started... */
int server_init() {
    struct sockaddr_in serv_addr;
    debug( "%s()\n", __PRETTY_FUNCTION__ );

    /** Set the server address. */
    serv_addr.sin_family = PF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    /** The same LWP must be used for IO signaling. */
    io_master = -getpid();

    /** Lock the server socket as they may be reinitialize at any time using the setopt
     */
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    pthread_mutex_lock( &stream_fd_mutex ); 
#endif
    if( stream_fd < 0 ) {
	/** Create the socket. */
	stream_fd = socket(PF_INET, SOCK_STREAM, 0);
    
	serv_addr.sin_port = htons( c_options.imago_server_tcp_port );
    
	/** Bind the TCP port. */
	if( bind( stream_fd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in) ) < 0 ) {
	    close( stream_fd );
	    stream_fd = -1;
	    dgram_fd = -1;
#ifdef _REENTRANT
	    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
	    pthread_mutex_unlock( &stream_fd_mutex ); 
#endif
	    return -1;
	}
    
	/** Set the descriptors to send signals. */
	fcntl( stream_fd, F_SETSIG, SIGIO );
	fcntl( stream_fd, F_SETFL,  O_ASYNC | O_NONBLOCK );  
	fcntl( stream_fd, F_SETOWN, io_master );
    
	/** Make the TCP side listen for connections. */
	listen( stream_fd, 5 );
    }
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    pthread_mutex_unlock( &stream_fd_mutex ); 
#endif

#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    pthread_mutex_lock( &dgram_fd_mutex ); 
#endif
    if( dgram_fd < 0 ) {
	dgram_fd = socket(PF_INET, SOCK_DGRAM, 0);
    
	serv_addr.sin_port = htons( c_options.imago_server_udp_port );
	/** Bind the UDP port. */
	if( bind( dgram_fd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in) ) < 0 ) {
	    close( dgram_fd );
	    close( stream_fd );
	    stream_fd = -1;
	    dgram_fd = -1;
#ifdef _REENTRANT
	    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
	    pthread_mutex_unlock( &dgram_fd_mutex ); 
#endif
	    return -1;
	}
    
	/** And now for the UDP socket. */
	fcntl( dgram_fd, F_SETSIG, SIGIO );
	fcntl( dgram_fd, F_SETFL,  O_ASYNC );
	fcntl( dgram_fd, F_SETOWN, io_master  );
    }
    /** */
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    pthread_mutex_unlock( &dgram_fd_mutex ); 
#endif

    notice( "Server listening on port TCP [%d] and UDP [%d].\n", 
	    c_options.imago_server_tcp_port, c_options.imago_server_udp_port ); 
    /** return success. */
    return 0;
}
/** */
void connection_put_signal(struct pdu_str *this_pdu) {
    debug("%s(%p)\n", __PRETTY_FUNCTION__, this_pdu );
    pdu_enqueue(this_pdu);
    transport_signal();
}

/** Initialization routine called once at the very first begining. */
void imago_layer_connection_init() {
    /** Register the states in the state machine. */
    imago_layer_connection_up_stateid = state_machine_register( (state_action_t)imago_layer_connection_up );
    imago_layer_connection_down_stateid = state_machine_register( (state_action_t) imago_layer_connection_down );
    connection_async_send_stateid = state_machine_register( (state_action_t) connection_async_send );

    /** Set the signal handler for the SIGIO. */
    imago_signal( SIGIO, SIGIO_handler, (void *)connection_put_signal );
    /** Set the signal handler for the SIGPIPE. */
    imago_signal( SIGPIPE, SIGPIPE_handler, NULL);

    /** Initialize the waiting to be sent pdu queue. */
    queue_init( &waiting_to_send );

    /** Initialize the connection module and the server side sockets. */
    if( con_init() < 0 || server_init() < 0 ) {
	critical( "Unable to initialize the Imago Server.\n" );
	exit(-1);
    }
    /** Set the connection timeout value. */
    con_settimeout(c_options.connection_timeout_sec);
}
/** */

/** Properly shutdown the server side sockets. 
    No new connection may be established.
    Active connection still remains until the connection_terminate is called
    or the connection naturally terminates. 
*/
void imago_layer_connection_shutdown() {
    /** terminate the server sockets. */  
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    pthread_mutex_lock( &stream_fd_mutex ); 
#endif
    if( stream_fd > 0 ) {
	notice( "Closing TCP Server.\n" );
	shutdown( stream_fd, SHUT_RDWR );
	close( stream_fd );
	stream_fd = -1;
    }
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    pthread_mutex_unlock( &stream_fd_mutex ); 
#endif
  
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    pthread_mutex_lock( &dgram_fd_mutex ); 
#endif
    if( dgram_fd > 0 ) {
	notice( "Closing UDP Server.\n" );
	shutdown( dgram_fd, SHUT_RDWR );
	close( dgram_fd );
	dgram_fd = -1;
    }
#ifdef _REENTRANT
    /** Protect the server socket from been tested and accessed by more than one thread at a time. */
    pthread_mutex_unlock( &dgram_fd_mutex ); 
#endif
}
/** */

/** Termination routine called (many times) at the end of the program. */
void imago_layer_connection_terminate() {
    /** terminate the server sockets. */  
    imago_layer_connection_shutdown();
    /** Terminate all connections. */
    con_end();
}

/** Protecting the c_options does not change anything to the fact that 
    many threads my change them in no particular order...
    In that case, the result is not known...
    But locking would not change anything...
*/
int imago_layer_connection_getopt( int opt , void *arg ) {
    switch( opt ) {
    case C_SERVER_PORT:
    case C_SERVER_TCP_PORT:
	((int *)arg) = c_options.imago_server_tcp_port;
	break;
    case C_SERVER_UDP_PORT:
	((int *)arg) = c_options.imago_server_udp_port;
	break;
    case C_TIMEOUT_SEC:
	((int *)arg) = c_options.connection_timeout_sec;
	break;
    default:
	return -1;
    }
    return 0;
}
int imago_layer_connection_setopt( int opt, void *arg ) {
    switch( opt ) {
    case C_SERVER_TCP_PORT:
    case C_SERVER_UDP_PORT:
    case C_SERVER_PORT:
	if( opt == C_SERVER_TCP_PORT || opt == C_SERVER_PORT )
	    c_options.imago_server_tcp_port = *((int *)arg);
	if( opt == C_SERVER_UDP_PORT || opt == C_SERVER_PORT )
	    c_options.imago_server_udp_port = *((int *)arg);
	/** For the server port, we need to restart the server sockets. */
	/** Only if they are already started (after init is called). */
	if( stream_fd > 0 || dgram_fd > 0 ) {
	    /** First we shut them down. */
	    imago_layer_connection_shutdown();
	    /** Then we restart the server sockets. */
	    return server_init();
	}
	break;
    case C_TIMEOUT_SEC:
	c_options.connection_timeout_sec = *((int *)arg);
	con_settimeout(c_options.connection_timeout_sec);
	break;
    default:
	return -1;
    }
    return 0;
}
/** */

state_id_t  imago_layer_connection_down_control( struct pdu_str *this_pdu ) {
    struct connection_header hdr;
    int udp_fd;
    con_t *cid = NULL;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 

    /** Complete the remote_addr structure. */
    this_pdu->remote_addr.sin_family = PF_INET;
    /** Set the server port. */
    this_pdu->remote_addr.sin_port = htons(c_options.imago_server_udp_port);

    /** Send the PDU using the UDP socket IFF no TCP connection to this destination is established. */
    cid = lookup_con( &this_pdu->remote_addr );
    /** First, send the Version information. */
    /** This is an Imago CONTROL packet. */
    /** Write the magic number. */
    hdr.magic = htonl(MAGIC_NUMBER);
    /** Send the Version MAJOR and MINOR. */ 
    hdr.version = htons(IMAGO_TRANSPORT_SICP_VERSION);
    /** Get the size of the complete PDU. */
    hdr.length = htonl(pdu_size(this_pdu));
    /** Retreive end send the PDU flags. */
    hdr.flags = htons(this_pdu->flags & ~PDU_FLAG_RESERVED);

    if( cid == NULL ) {
	/*** Sending the PDU across the network. */
	if( (udp_fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0 ) {
	    /** This is a SICP PDU, errors are silently ignored. */
	    warning( "Unable to create UDP socket.\n" );
	    goto error;
	}
	/** And connect it to the remote peer. */
	if( connect(udp_fd, (struct sockaddr *)&this_pdu->remote_addr, sizeof(struct sockaddr_in)) < 0 ) {
	    /** This is a SICP PDU, errors are silently ignored. */
	    warning( "Unable to connect the UDP socket.\n" );
	    close( udp_fd );
	    goto error;
	}  
    } else {
	/** Retreive the socket id. */
	udp_fd = get_csocket( cid );
	con_update( cid );
    }
    {
	int size = pdu_size( this_pdu );
	char buf[size + sizeof(struct connection_header)];
	/** Copy the connection header into the buffer. */
	memcpy( buf, &hdr, sizeof(hdr) );
	/** Then, copy the remaining part of the PDU. */
	pdu_read_string( this_pdu, &buf[sizeof(hdr)], size ); 
	/** Send the PDU at once. */
	send( udp_fd, buf, sizeof(buf), 0 );
    }
    /** */
    /** Close or release the connection. */
    if( cid == NULL ) close( udp_fd );
    else con_unlock( cid );
 error:
    /** If the PDU has been sent (or not), it shall be discarded automatically. */
    pdu_free( this_pdu );
    return STATE_MACHINE_ID_INVALID;
}

state_id_t  imago_layer_connection_down_transport( struct pdu_str *this_pdu ) {
    int sockopt = 0;
    con_t *cid;
    struct connection_header hdr;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 

    /** Complete the remote_addr structure. */
    this_pdu->remote_addr.sin_family = PF_INET;
    /** The server port may be set from up above but if not (ie: 0) set it to some default. */
    this_pdu->remote_addr.sin_port = htons(c_options.imago_server_tcp_port);

    /** Lookup in the connection table if there is anything going to this destination. */
    if( (cid = lookup_con(&this_pdu->remote_addr)) == NULL ) {
	/** No connection available now. */
	if( (cid = make_con(NULL, &this_pdu->remote_addr, io_master)) == NULL ) {
	    /** Could not connect to this host. This is a fact. */
	    warning("PDU Count not be sent.\n" );
	    this_pdu->errno = EPDU_CONNREFUSED;
	    goto error;
	}
    }
    /** Update the connection specific fields. */
    this_pdu->c_id = cid;
    this_pdu->c_total = pdu_size(this_pdu); 
    /** First, send the Version information. If it is a TRANSFER or CONTROL */
    if( (this_pdu->flags & PDU_TYPE_SITP) ) {
	/** This is an Imago TRANSPORT packet. */
	/** Send the Version MAJOR and MINOR. */ 
	hdr.version = htons(IMAGO_TRANSPORT_SITP_VERSION);
    } else {
	/** This is an Imago CONTROL packet. */
	/** Send the Version MAJOR and MINOR. */ 
	hdr.version = htons(IMAGO_TRANSPORT_SICP_VERSION);
    }
    /** Write the magic number. */
    hdr.magic = htonl(MAGIC_NUMBER);
    /** Retreive end send the PDU flags. */
    hdr.flags = htons(this_pdu->flags & ~PDU_FLAG_RESERVED);
    /** Get the size of the complete PDU. */
    hdr.length = htonl( this_pdu->c_total );
    /** Turn the AUTO PUSH flag off on this socket. */
    sockopt = 1;
    setsockopt( get_csocket(cid), SOL_TCP, TCP_CORK, &sockopt, sizeof(sockopt) );  
    /*** Sending the PDU across the network. */
    /** Send the header information first. */
    if( send( get_csocket(cid), &hdr, sizeof(hdr), 0 ) < 0 ) {
	/** May be the connection has been closed inexpectedly. 
	    Just try again right away. The above syscall would have
	    most likely fired a SIGPIPE if the connection was broken.
	    There is no chance the PDU get stuck retrying for ever because 
	    the State_machine will get rid of it if it goes through the same 
	    state too many times. */
	sockopt = 0;
	setsockopt( get_csocket(cid), SOL_TCP, TCP_CORK, &sockopt, sizeof(sockopt) );  
	con_unlock( cid );
	return imago_layer_connection_down_stateid;
    }
    /** Process the main body appart. */
    return connection_async_send_stateid;

 error:
    /** In this case, if the PDU is ours, we send it back to the upper layers.
	Otherwise, we destroy it right away. */
    if( this_pdu->flags & PDU_FLAG_FORWARDING ) {
	/** Not from this machine... */
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;    
    } else {
	/** This is ours. */
	this_pdu->flags |= PDU_ERROR;
	this_pdu->e_layer = imago_layer_connection_down_stateid;
	return imago_layer_routing_up_stateid;
    }
}

state_id_t  connection_async_send( struct pdu_str *this_pdu ) {
    /** This routine asynchronously send the data.
	It must be done in a none blocking stage because otherwise, 
	server sending to one another may generate a dead lock.
	All threads may be stuck trying to send data over the network while 
	no threads my be available for receiving... */
    int count, sockopt = 0;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 

    /** The pdu header has been send already. 
	just continue sending the main body. */
    /** Send all the frame making this PDU. */
    count = pdu_send_frame( get_csocket((con_t *)this_pdu->c_id), this_pdu );
    /*** */
    pdu_erase_frame( this_pdu, count );
    /** Decrement the total number of bytes. */
    this_pdu->c_total = this_pdu->c_total - (long)count;
    debug("Connection sending %d bytes (%d remaining [%d])\n", count, this_pdu->c_total, pdu_size(this_pdu) );
    /** If there are more to go, just do it. */
    if( this_pdu->c_total > 0 ) {
	/** count may or may not be 0. */
	if( !count ) {
	    enqueue( &waiting_to_send, &this_pdu->link );
	    return STATE_MACHINE_ID_INVALID;
	}
	return connection_async_send_stateid;
    }
    /** Else, terminate and release the connection. */
    /** Turn the AUTO PUSH flag back on, on this socket. */
    sockopt = 0;
    setsockopt( get_csocket((con_t *)this_pdu->c_id), SOL_TCP, TCP_CORK, &sockopt, sizeof(sockopt) );
    con_update( this_pdu->c_id );
    /** Release the connection. */
    con_unlock( this_pdu->c_id );
    /** If the PDU has been sent, it shall be discarded automatically. */
    pdu_free( this_pdu );
    return STATE_MACHINE_ID_INVALID;
}

state_id_t imago_layer_connection_down( struct pdu_str *this_pdu ) {
    /** Handle this differently if this is a SICP PDU. */
    if( !(this_pdu->flags & PDU_TYPE_SITP) && (pdu_size(this_pdu) < UDP_MTU) ) 
	/** Control frame are sent using the UDP protocol, unless it is too big for it... */
	return imago_layer_connection_down_control( this_pdu );
    else
	return imago_layer_connection_down_transport( this_pdu );
}

state_id_t imago_layer_connection_up( struct pdu_str *this_pdu ) {
    struct connection_header hdr;
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
    /** Read the version number. */
    pdu_read_string( this_pdu, (char *)&hdr, sizeof(hdr) );
    /** Switch some bytes arround... */
    hdr.magic = ntohl( hdr.magic );
    hdr.version = ntohs( hdr.version );
    hdr.flags = ntohs( hdr.flags );
    hdr.length = ntohl( hdr.length );
    /** Check the magic number first. */
    if( hdr.magic != MAGIC_NUMBER ) {
	error("Bad Magic Number in packet.\n" );
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    }
    /** Set the flags on this PDU. */
    this_pdu->flags = hdr.flags;
    /** Test the version against what we support. */
    if( hdr.flags & PDU_TYPE_SITP ) {
	/** 
	 * TO DO: Find a better way to handle version. 
	 */
	if( IMAGO_TRANSPORT_SITP_VERSION != hdr.version ) {
	    warning( "Invalid SITP Version Number for Incoming PDU (is: %d.%d, should be: %d.%d).\n",
		     (hdr.version >> IMAGO_TRANSPORT_MAJOR_SHIFT) & 0xff, hdr.version & 0xff, 
		     IMAGO_TRANSPORT_SITP_MAJOR, IMAGO_TRANSPORT_SITP_MINOR
		     );
	    pdu_free( this_pdu );
	    return STATE_MACHINE_ID_INVALID;
	}
    } else {
	/** 
	 * TO DO: Find a better way to handle version. 
	 */
	if( IMAGO_TRANSPORT_SICP_VERSION != hdr.version ) {
	    warning( "Invalid SICP Version Number for Incoming PDU (is: %d.%d, should be: %d.%d).\n",
		     (hdr.version >> IMAGO_TRANSPORT_MAJOR_SHIFT) & 0xff, hdr.version & 0xff, 
		     IMAGO_TRANSPORT_SICP_MAJOR, IMAGO_TRANSPORT_SICP_MINOR
		     );
	    pdu_free( this_pdu );
	    return STATE_MACHINE_ID_INVALID;
	}
    }
    /** This is basically all the connection layer needs to do for now... */
    return imago_layer_routing_up_stateid;
}
