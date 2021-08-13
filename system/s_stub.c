/** $RCSfile: s_stub.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: s_stub.c,v 1.16 2003/03/24 14:07:48 gautran Exp $ 
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
/** Server Stub code. 
    This part takes care of the interactions from the user command line.
    A UNIX socket is created to listen on a specific port and execute user commands
    like print logs, start and stop queens, kill worker, etc...
*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/un.h>

#include <errno.h>

#include "version.h"
#include "log.h"
#include "s_stub.h"
#include "imago_signal.h"
#include "system.h"
#include "imago_icb.h"

extern char *__progname;

struct context_cmd c_l1_cmd[] = {
    { {"ADMN"}, "Switch to the administrator command level." },
    { {"CVER"}, "Print the Client Version Information." },
    { {"EXIT"}, "Exit this command line." },
    { {"HELP"}, "Display a command summary with description of the command provided in argument." },
    { {"KILL"}, "Kill an application." },
    { {"LOAD"}, "Load and start an Imago Queen (absolute path filename as command argument)." },
    { {"PDIS"}, "Print the Software Disclaimer information." },
    { {"PLIC"}, "Print the Software License information." },
    { {"PRNT"}, "Print the server log content." },
    { {"QUIT"}, "Quit the command line." },
    { {"SVER"}, "Print the Server Version Information." },
    /** Must remain last entry in the table. */
    { {i_cmd: C_CMD_NULL}, NULL } 
};
struct context_cmd c_l2_cmd[] = {
    { {"CVER"}, "Print the Client Version Information." },
    { {"EXIT"}, "Exit the administrator command level." },
    { {"HELP"}, "Display a command summary with description of the command provided in argument." },
    { {"KILL"}, "Kill any imago or application." },
    { {"LOAD"}, "Load and start an Imago Queen (absolute path filename as command argument)." },
    { {"PDIS"}, "Print the Software Disclaimer information." },
    { {"PLIC"}, "Print the Software License information." },
    { {"PRNT"}, "Print the server log content." },
    { {"QUIT"}, "Quit the command line." },
    { {"SLCT"}, "Select the application ID provided as argument as the current application.\n"
                " Syntax:  'SLCT gen-app'.\n Example: 'SLCT 18175-0'." },
    { {"SVER"}, "Print the Server Version Information." },
    /** Must remain last entry in the table. */
    { {i_cmd: C_CMD_NULL}, NULL } 
};
struct context_cmd c_l3_cmd[] = {
    { {"HELP"}, "Display a command summary with description of the command provided in argument." },
    /** Must remain last entry in the table. */
    { {i_cmd: C_CMD_NULL}, NULL } 
};



/** Internal function declarations. */
void s_stub_sig_handler( int, void * );
int s_stub_unix_poll();
int c_stub_unix_poll();
int context_invalid( struct c_stub * );
int context_closing( struct c_stub * );
int context_l1( struct c_stub * );
int context_l2( struct c_stub * );
int context_l3( struct c_stub * );
int context_prnt_log( struct c_stub * );
int context_load( struct c_stub *, char * );
int context_kill( struct c_stub *, int );
int context_license( struct c_stub * );
int context_disclaimer( struct c_stub * );
char *get_filename( char * );
/** */



struct c_stub cli_stubs[MAX_CLI];
/** The stubs lock is used for both, locking the cli_stubs structures and 
    at the same time the server socket. */
pthread_mutex_t unix_fd_lock;
int unix_fd = -1;
struct sockaddr_un local_addr;
pid_t l_io_master = -1;

/** What signal do we request to receive when data are comming in ???
    SIGUSR2 is been used already by the transport stack, so we choose SIGUSR1. */
const int SIG_s_stub = SIGUSR1;


int (*context_fcnt[])( struct c_stub * ) = {
    context_invalid,
    context_closing,
    context_l1,
    context_l2,
    context_l3
};


int s_stub_init( int l_port ) {
    int i;
    /** First, initialize the client stub array. */
    debug( "%s(%d)\n", __PRETTY_FUNCTION__, l_port );
    /** Clean it up first. */
    memset( cli_stubs, 0, MAX_CLI * sizeof(struct c_stub) );
    /** All clean, initialize the mutexes. */
    for( i = 0; i < MAX_CLI; i++ ) {
	/** Init the mutex. */
	pthread_mutex_init( &cli_stubs[i].lock, NULL );
	/** Init the sockets. */
	cli_stubs[i].fd = -1;
    }
    /** Initialize the stubs lock. */
    pthread_mutex_init( &unix_fd_lock, NULL );

    /** Create the UNIX socket. */
    if( (unix_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0 ) {
	warning( "Unable to create a local UNIX socket.\n" );
	return 0;
    }

    /** Set the local server address. */
    snprintf( local_addr.sun_path, UNIX_PATH_MAX, "%s/mlvmd_%d", MLVM_TMPDIR, l_port);
    local_addr.sun_family = PF_UNIX;

    /** The same LWP can be used for IO signaling. */
    l_io_master = -getpid();

    /** Bind the TCP port. */
    if( bind( unix_fd, (struct sockaddr*)&local_addr, sizeof(struct sockaddr_un) ) < 0 ) {
	info( "Socket file existing (Another server may be running).\n" );
	/** Remove the stall file. */
	unlink( local_addr.sun_path );
	/** Try again. */
	if( bind( unix_fd, (struct sockaddr*)&local_addr, sizeof(struct sockaddr_un) ) < 0 ) {
	    close( unix_fd );
	    unix_fd = -1;
	    warning( "Unable to bind server to local UNIX socket (%s).\n", strerror( errno ) );
	    return 0;
	}
    }
    /** Set the descriptors to send signals. */
    fcntl( unix_fd, F_SETSIG, SIG_s_stub );
    fcntl( unix_fd, F_SETFL,  FASYNC | O_NONBLOCK );  
    fcntl( unix_fd, F_SETOWN, l_io_master );
  
    /** Make the TCP side listen for connections. */
    listen( unix_fd, 5 );

    chmod( local_addr.sun_path, S_IRUSR | S_IWUSR |  S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    /** And we are off ! ! ! */
    notice( "Server listening on local UNIX port [%d]\n", l_port );

    /** Register the signal handler. The transport stack MUST be initialized first... */
    imago_signal( SIG_s_stub, s_stub_sig_handler, NULL );

    return 1;
}

void s_stub_cleanup() {
    int i;
    /** Close all opened sockets. */
    pthread_mutex_lock( &unix_fd_lock );
    shutdown( unix_fd, SHUT_RDWR );
    close( unix_fd );
    unix_fd = -1;
    unlink( local_addr.sun_path );
    pthread_mutex_unlock( &unix_fd_lock );

    for( i = 0; i < MAX_CLI; i++ ) {
	/** Lock the entry. */
	pthread_mutex_lock( &cli_stubs[i].lock ); 
	/** Make sure this guy is currently used. */
	if( cli_stubs[i].context != CONTEXT_INVALID ) {
	    shutdown( cli_stubs[i].fd, SHUT_RDWR );
	    close( cli_stubs[i].fd );
	    cli_stubs[i].fd = -1;
	    cli_stubs[i].context = CONTEXT_INVALID;
	}
	pthread_mutex_unlock( &cli_stubs[i].lock );
    }
}

void s_stub_sig_handler(int sig, void *arg) {
    debug( "%s(%d, %p)\n", __PRETTY_FUNCTION__, sig, arg );
    /** Poll the server socket for incoming connections. */
    s_stub_unix_poll();
    /** Poll client socket for incoming requests. */
    c_stub_unix_poll();
}
int s_stub_proc() {
    /** Poll client socket for incoming requests. */
    return c_stub_unix_poll();
}


/** Poll the server for incoming client connections. */
int s_stub_unix_poll() {
    struct sockaddr_un c_addr;
    socklen_t c_addrlen = sizeof( c_addr );
    int c_fd, i, ret;

    /** Lock up the server socket for pollings (No need to keep'n waiting). */
    if( pthread_mutex_trylock(&unix_fd_lock) != 0 ) return 0;
    /** Test if there are new connection to be accepted. */
    while( (c_fd = accept( unix_fd, &c_addr, &c_addrlen )) >= 0 ) {
	/** Initialize the connection strait away. */
	fcntl( c_fd, F_SETSIG, SIG_s_stub );
	fcntl( c_fd, F_SETFL,  FASYNC );
	fcntl( c_fd, F_SETOWN, l_io_master );

	/** Find a slot for it. */
	for( i = 0; i < MAX_CLI; i++ ) {
	    if( pthread_mutex_trylock(&cli_stubs[i].lock) == 0 ) {
		/** The structure is ours. */
		if( cli_stubs[i].context == CONTEXT_INVALID ) {
		    /** Free slot, use it. */
		    cli_stubs[i].fd = c_fd;
		    cli_stubs[i].context = CONTEXT_L1; /** Regular client. */
		    cli_stubs[i].application = 0;
		    write( c_fd, "\n", 1 ); /** A new line. */
		    write( c_fd, S_STUB_GREETING, sizeof(S_STUB_GREETING)-1 );
		    pthread_mutex_unlock( &cli_stubs[i].lock );
		    notice( "A new local connection has been accepted.\n" );
		    break;/** Get out of the for loop. */
		}
		pthread_mutex_unlock( &cli_stubs[i].lock );
	    }
	}
	ret = 1;
	/** Out of client stubs ??? Too bad... */
	if( i >= MAX_CLI ) {
	    shutdown( c_fd, SHUT_RDWR );
	    close( c_fd );
	    ret = 0;
	    /** No need to accept any other connections since we ran out of slots already... */
	    break;
	}
    }
    /** Release the server socket. */
    pthread_mutex_unlock( &unix_fd_lock );
    return ret;
}

/** Poll client stubs for incoming requests. */
int c_stub_unix_poll() {
    fd_set  readfds, orgfds;
    struct timeval timeout;
    int high_fd = -1, ret = 0, i;

    /** Clear the fdset. */
    FD_ZERO(&readfds);
    FD_ZERO(&orgfds);

    /** Time out of 0 prevent the select from blocking indefinitely... */
    memset( &timeout, 0, sizeof(struct timeval) );

    /** Grab as many lock as you can and process them all. */
    for( i = 0; i < MAX_CLI; i++ ) {
	/** Try locking the entry. */
	if( pthread_mutex_trylock(&cli_stubs[i].lock) == 0 ) {
	    /** Make sure this guy is currently used. */
	    if( cli_stubs[i].context != CONTEXT_INVALID ) {
		/** If so, add the fd to the fdset. */
		FD_SET( cli_stubs[i].fd, &readfds );
		FD_SET( cli_stubs[i].fd, &orgfds );
		/** Update the higher fd number. */
		high_fd = (high_fd < cli_stubs[i].fd) ? cli_stubs[i].fd: high_fd;
	    } else {
		pthread_mutex_unlock( &cli_stubs[i].lock );
	    }
	}
    }
    /** We may have grabed many things or nothing at all... */
    if( high_fd < 0 ) return 0;
    /** Perform our select on all we got. */
    ret = select( high_fd + 1, &readfds, NULL, NULL, &timeout );

    /** See what has been changed... */
    for( i = 0; i < MAX_CLI; i++ ) {
	if( cli_stubs[i].context == CONTEXT_INVALID )
	    continue;
	/** Did we grab that lock ?? */
	if( FD_ISSET(cli_stubs[i].fd, &orgfds) ) {
	    /** If the select failed (or nothing changed), we need not to proceed further. */
	    if( ret > 0 ) {
		/** Did this guy changed ?? */
		if( FD_ISSET(cli_stubs[i].fd, &readfds) ) {
		    /** Yes, then process it on site.
			Here we do not need to worry about starvation, the cli is low priority and will not suck up the CPU. */
		    cli_stubs[i].context = context_fcnt[cli_stubs[i].context]( &cli_stubs[i] );
		}
	    }
	    pthread_mutex_unlock( &cli_stubs[i].lock );
	}
    }
    return ret;
}

int context_invalid( struct c_stub *c ) {
    error("%s(%p): Should not have happened !\n", __PRETTY_FUNCTION__, c );
    context_closing( c );
    return CONTEXT_INVALID;
}

int context_closing( struct c_stub *c ) {
    debug("%s(%p)\n", __PRETTY_FUNCTION__, c );
    shutdown( c->fd, SHUT_RDWR );
    close( c->fd );
    c->fd = -1;
    c->application = 0;
    return CONTEXT_INVALID;
}

void context_g_help( int c_fd, struct context_cmd *c_cmd ) {
    int i, count;
    /** Display the content or the context command table. */
    /** Print header. */
    write( c_fd, HELP_HEADER, sizeof(HELP_HEADER)-1 );
    for( i = 0, count = 0; c_cmd[i].u_cmd.i_cmd != C_CMD_NULL; i++ ) {
	/** New line from time to time... */
	if( count == 0 ) write( c_fd, "\n", 1 );
	count = (count + 1) % 8; /** Eight commands per line should be good enough. */
	write( c_fd, "  ", 2 ); /** Two spaces for a start. */
	write( c_fd, c_cmd[i].u_cmd.s_cmd, 4 ); /** The command as a ascii string. */
    }
    write( c_fd, "\n\n", 2 ); /** A new line. */
    /** Print footer. */
    write( c_fd, HELP_FOOTER, sizeof(HELP_FOOTER)-1 );


}
void context_help( int c_fd, struct context_cmd *c_cmd, char *arg ) {
    int i;
    struct context_cmd cmd;
    /** Display the content or the context command table if the arg is NULL 
	or the command descritpion if the arg is found. */
    if( arg == NULL || strlen(arg) <= 0 || strlen(arg) > 4) { /** General help display. */
	context_g_help( c_fd, c_cmd );
	return;
    }
    /** Copy the command into the temporary structure. */
    memcpy( cmd.u_cmd.s_cmd, arg, sizeof(cmd.u_cmd.s_cmd) );
    /** Switch bytes arround for intel platform. */

    debug( "The HELP is on: '%s' (0x%X)\n", arg, cmd.u_cmd.s_cmd, cmd.u_cmd.i_cmd );


    for( i = 0; c_cmd[i].u_cmd.i_cmd != C_CMD_NULL; i++ ) {
	if( c_cmd[i].u_cmd.i_cmd == cmd.u_cmd.i_cmd ) {
	    /** Otherwise, print the context for this command. */
	    write( c_fd, HELP_HEADER, sizeof(HELP_HEADER)-1 );
	    write( c_fd, "\n", 1 ); /** A new line. */
	    write( c_fd, "  ", 2 ); /** Two spaces for a start. */
	    write( c_fd, c_cmd[i].u_cmd.s_cmd, 4 ); /** The command as a ascii string. */
	    write( c_fd, "\n", 1 ); /** A new line. */
	    write( c_fd, c_cmd[i].desc, strlen(c_cmd[i].desc) ); /** The command description.*/
	    write( c_fd, "\n\n", 2 ); /** A new line. */
	    /** Print footer. */
	    write( c_fd, HELP_FOOTER, sizeof(HELP_FOOTER)-1 );
	    break;
	}
    }
    if( c_cmd[i].u_cmd.i_cmd == C_CMD_NULL ) 
	context_g_help( c_fd, c_cmd );
}


int context_l1( struct c_stub *c ) {
    char buf[256];/** Long enough for one line. */
    int i;
    struct context_cmd cmd = { {i_cmd: C_CMD_NULL}, NULL };
    debug("%s(%p)\n", __PRETTY_FUNCTION__, c );

    /** Read one character at a time 
	(very inefficient). */
    for( i = 0; i < sizeof(buf); i++ ) {
	if( read( c->fd, &buf[i], 1 ) <= 0 ) /** Close this socket cause it's dead. */
	    return context_closing( c );
	/** Stop at a newline. */
	if( buf[i] == '\n' ) {
	    /** Strip the newline. */
	    buf[i] = 0x00;
	    break;
	}
    }
    /** Copy the command into the temporary structure. */
    memcpy( cmd.u_cmd.s_cmd, buf, sizeof(cmd.u_cmd.s_cmd) );

    /** Switch bytes arround for intel platform so they are both screwed up. */
    cmd.u_cmd.i_cmd = ntohl(cmd.u_cmd.i_cmd);

    debug( "Command is: '%s' (0x%X)\n", cmd.u_cmd.s_cmd, cmd.u_cmd.i_cmd );
  
    /** Switch the command. */
    switch( cmd.u_cmd.i_cmd ) {
    case C_CMD_ADMN:
	write( c->fd, L2_START, sizeof(L2_START)-1 );
	return CONTEXT_L2;
	break;
    case C_CMD_EXIT:
    case C_CMD_QUIT:
	write( c->fd, L1_QUIT, sizeof(L1_QUIT)-1 );
	return context_closing( c );
	break;
    case C_CMD_PRNT:
	context_prnt_log( c );
	break;
    case C_CMD_KILL:
	/** Invalid command if no Imago loaded first. */
	if( !c->application ) {
	    write( c->fd, KILL_L1_RESTRICT, sizeof(KILL_L1_RESTRICT)-1 );
	    break;
	}
	context_kill( c, c->application );
	break;
    case C_CMD_LOAD:
	/** Invalid command if already used. */
	if( c->application ) {
	    /** Before complaining, check if the application as terminated properly. */
	    LOGPtr log = NULL;

	    log_lock();
	    for( log = live_logs->head; log; log = log->next) {
		if( c->application == log->application && log->ip && log->ip->type == STATIONARY ) {
		    break;
		}
	    }
	    log_unlock();
	    /** Did we find the queen ??? */
	    if( log ) {
		/** If we have the log pointer to the queen, it means she is not dead yet... */
		write( c->fd, LOAD_RESTRICT, sizeof(LOAD_RESTRICT)-1 );
		break;
	    }
	}
	/** This command requires one argument that is the imago queen file name. */
	i = 4; /** Skip the command. */
	while( buf[i] == ' ' || buf[i] == '\t' ) i++; /** Skip the spaces. */
	/** The imago file name MUST be provided as argument of the LOAD command. */
	if( strlen(&buf[i]) <= 0 ) {
	    /** Display the help for this command. */
	    buf[4] = 0x00;
	    context_help(c->fd, c_l1_cmd, buf ); /** Invoke the help on the command. */
	    break;
	}
	/** Load the imago. */
	context_load(c, &buf[i] ); 
	break;
    case C_CMD_HELP:
	write( c->fd, HELP_INVOKE, sizeof(HELP_INVOKE)-1 );
	i = 4; /** Skip the command. */
	while( buf[i] == ' ' || buf[i] == '\t' ) i++; /** Skip the spaces. */
	context_help(c->fd, c_l1_cmd, &buf[i] ); /** Invoke the help on the command. */
	break;
    case C_CMD_PLIC:
	context_license( c );
    case C_CMD_PDIS:
	context_disclaimer( c );
	break;
    case C_CMD_SVER: {
	char version[80];
	snprintf( version, sizeof(version)-1, MLVM_NAME "(%s) version " MLVM_EXT_VERSION "\n", __progname );
	write( c->fd, version, strlen(version) );
	write( c->fd, MLVM_COPYRIGHT, sizeof(MLVM_COPYRIGHT)-1 );
	break;
    }
    case C_CMD_CVER:
    default:
	write( c->fd, S_STUB_ERROR, sizeof(S_STUB_ERROR)-1 );
	break;
    }
 
    return CONTEXT_L1;
}
int context_l2( struct c_stub *c ) {
    char buf[256];/** Long enough for one line. */
    int i;
    struct context_cmd cmd = { {i_cmd: C_CMD_NULL}, NULL };
    debug("%s(%d)\n", __PRETTY_FUNCTION__, c->fd );

    /** Read one character at a time 
	(very inefficient). */
    for( i = 0; i < sizeof(buf); i++ ) {
	if( read( c->fd, &buf[i], 1 ) <= 0 ) /** Close this socket cause it's dead. */
	    return context_closing( c );
	/** Stop at a newline. */
	if( buf[i] == '\n' ) {
	    /** Strip the newline. */
	    buf[i] = 0x00;
	    break;
	}
    }
    /** Copy the command into the temporary structure. */
    memcpy( cmd.u_cmd.s_cmd, buf, sizeof(cmd.u_cmd.s_cmd) );

    /** Switch bytes arround for intel platform. */
    cmd.u_cmd.i_cmd = ntohl(cmd.u_cmd.i_cmd);

    debug( "Command is: '%s' (0x%X)\n", cmd.u_cmd.s_cmd, cmd.u_cmd.i_cmd );
  
    /** Switch the command. */
    switch( cmd.u_cmd.i_cmd ) {
    case C_CMD_QUIT:
	write( c->fd, L2_QUIT, sizeof(L2_QUIT)-1 );
	return context_closing( c );
	break;
    case C_CMD_EXIT:
	write( c->fd, L2_STOP, sizeof(L2_STOP)-1 );
	return CONTEXT_L1;
	break;
    case C_CMD_PRNT:
	context_prnt_log( c );
	break;
    case C_CMD_KILL:
	/** Invalid command if no Imago loaded first. */
	if( !c->application ) {
	    write( c->fd, KILL_L2_RESTRICT, sizeof(KILL_L2_RESTRICT)-1 );
	    break;
	}
	context_kill( c, c->application );
	break;
    case C_CMD_LOAD:
	/** This command requires one argument that is the imago queen file name. */
	i = 4; /** Skip the command. */
	while( buf[i] == ' ' || buf[i] == '\t' ) i++; /** Skip the spaces. */
	/** The imago file name MUST be provided as argument of the LOAD command. */
	if( strlen(&buf[i]) <= 0 ) {
	    /** Display the help for this command. */
	    buf[4] = 0x00;
	    context_help(c->fd, c_l2_cmd, buf ); /** Invoke the help on the command. */
	    break;
	}
	/** Load the imago. */
	context_load(c, &buf[i] ); 
	break;
    case C_CMD_SLCT: {
	int gen = 0, app = 0;
	/** This command requires one argument that is the imago queen application ID. */
	i = 4; /** Skip the command. */
	while( isspace(buf[i]) ) i++; /** Skip the spaces. */
	/** The imago application ID MUST be provided as argument of the SLCT command. */
	if( sscanf(&buf[i], "%d-%d", &gen, &app) != 2 ) {
	    /** Display the help for this command. */
	    buf[4] = 0x00;
	    context_help(c->fd, c_l2_cmd, buf ); /** Invoke the help on the command. */
	    break;
	}
	/** Syntax is something like 'SLCT gen-app' example: 18175-0. */
	c->application = (gen << (sizeof(short)*8)) | app;
	write( c->fd, SLCT_FOOTER, sizeof(SLCT_FOOTER)-1 );
	break;}
    case C_CMD_HELP:
	write( c->fd, HELP_INVOKE, sizeof(HELP_INVOKE)-1 );
	i = 4; /** Skip the command. */
	while( buf[i] == ' ' || buf[i] == '\t' ) i++; /** Skip the spaces. */
	context_help(c->fd, c_l2_cmd, &buf[i] ); /** Invoke the help on the command. */
	break;
    case C_CMD_PLIC:
	context_license( c );
    case C_CMD_PDIS:
	context_disclaimer( c );
	break;
    case C_CMD_SVER: {
	char version[80];
	snprintf( version, sizeof(version)-1, MLVM_NAME "(%s) version " MLVM_VERSION "\n", __progname );
	write( c->fd, version, strlen(version) );
	write( c->fd, MLVM_COPYRIGHT, sizeof(MLVM_COPYRIGHT)-1 );
	break; 
    }
    case C_CMD_CVER:
    default:
	write( c->fd, S_STUB_ERROR, sizeof(S_STUB_ERROR)-1 );
	break;
    }
    return CONTEXT_L2;
}
int context_l3( struct c_stub *c ) {
    debug("%s(%p)\n", __PRETTY_FUNCTION__, c->fd );

    context_help(c->fd, c_l3_cmd, NULL );
    return CONTEXT_L3;
}

int context_prnt_log( struct c_stub *c ) {
    LOGPtr log;
    char buf[1024]; /** Hold one line in the log entry. */
    int len = 0;

    /** Print log header. */
    write( c->fd, LOG_HEADER, sizeof(LOG_HEADER)-1 );
    log_lock();
    for( log = live_logs->head; log; log = log->next) {
	len = 0;
	if( c->context == CONTEXT_L1 ) {
	    if( c->application != log->application ) 
		continue;
	} else len = snprintf( buf, sizeof(buf)-1, "Application: %d-%d, ", 
			       log->application >> (sizeof(short)*8), log->application & 0xffff );

	len += snprintf( &buf[len], sizeof(buf)-(len+1), "Imago: %s, Jumps: %d, Status: ", log->name, log->jumps );

	switch( log->status ) {
	case LOG_MOVED:
	    len += snprintf( &buf[len], sizeof(buf)-(len+1), "Moved('%s')", log->server.name );
	    break;
	case LOG_ALIVE:
	    len += snprintf( &buf[len], sizeof(buf)-(len+1), "Alive" );
	    break;
	case LOG_DISPOSED:
	    len += snprintf( &buf[len], sizeof(buf)-(len+1), "Disposed" );
	    break;
	case LOG_BLOCKED:
	    len += snprintf( &buf[len], sizeof(buf)-(len+1), "Blocked" );
	    break;
	case LOG_SLEEPING:
	    len += snprintf( &buf[len], sizeof(buf)-(len+1), "Sleeping" );
	    break;
	default:
	    len += snprintf( &buf[len], sizeof(buf)-(len+1), "(Unknown)" );
	    break;
	}
	/** Finish the log entry with a new line. */
	buf[len] = '\n';
	/** Write it to the client (Do not forget the new line :). */
	write( c->fd, buf, len + 1 );
    }
    log_unlock();
    /** Write the footer. */
    write( c->fd, LOG_FOOTER, sizeof(LOG_FOOTER)-1 );

    return c->context;
}

int context_load( struct c_stub *c, char *filename ) {
    ICBPtr ip;
    int fd;
    struct stat bstat;

    write( c->fd, LOAD_HEADER, sizeof(LOAD_HEADER)-1 );
    /** Check if this file exists and can be loaded. */
    if( ((fd = open(filename, O_RDONLY)) < 0) || (fstat(fd, &bstat) < 0) || !S_ISREG(bstat.st_mode) ) {
	write( c->fd, LOAD_FAILURE1, sizeof(LOAD_FAILURE1)-1 );
	if( fd >= 0 ) close(fd);
    } else {
	close( fd );
	/** Make the engine create the IMAGO. */
	if( (ip = icb_new()) == NULL ) {
	    write( c->fd, LOAD_FAILURE2, sizeof(LOAD_FAILURE2)-1 );
	} else {
	    char *file = get_filename( filename );
	    notice( "Loading Imago file '%s'...\n", filename );
	    strncpy(ip->name, file, sizeof(ip->name)-1); /** Only copy the file name in here. */
	    *file = 0x00; /** Extract the file path from the filename. */
	    strncpy(ip->citizenship.name, filename, sizeof(ip->citizenship.name)-1); /** Copy the path to the file in here. */
	    /** Get this imago ready for creation. */
	    ip->status = IMAGO_CREATION;
	    ip->cin  = c->fd;
	    ip->cout = c->fd;
	    ip->cerr = c->fd;
	    c->application = ip->application = get_unique_appid();
	    icb_put_signal(creator_queue, ip);
	    write( c->fd, LOAD_SUCCESS, sizeof(LOAD_SUCCESS)-1 );

	}
    }
    write( c->fd, LOAD_FOOTER, sizeof(LOAD_FOOTER)-1 );
    return c->context;
}

int context_kill( struct c_stub *c, int application ) {
    /** Try to kill the named application. */
    LOGPtr log = NULL;
    ICBPtr tip = NULL;

    /** Print log header. */
    write( c->fd, KILL_HEADER, sizeof(KILL_HEADER)-1 );
    log_lock();
    for( log = live_logs->head; log; log = log->next) {
	if( application == log->application && log->ip && log->ip->type == STATIONARY ) {
	    tip = log->ip;
	    break;
	}
    }
    log_unlock();
    /** Did we find our queen ??? */
    if( tip ) {
	/** Lock it as fast as we can ! */
	icb_lock( tip );
	/** If this little girl is gone before we had a chance to get her... */
	if( !(tip->status == IMAGO_INVALID || tip->application != application || tip->type != STATIONARY) ) {
	    /** Properly & safely kill that guy. */
	    dispose_of( tip );
	    /** We do not check the return value 'cause we are lazy... */
	    write( c->fd, KILL_SUCCESS, sizeof(KILL_SUCCESS)-1 );
	    /** Reset the application ID. */
	    c->application = 0;
	} else {
	    /** Oops ! ! She's gone ! :( */
	    write( c->fd, KILL_FAILURE1, sizeof(KILL_FAILURE1)-1 );
	    /** Reset the application ID anyways. */
	    c->application = 0;
	}
	icb_unlock( tip );
    } else {
	write( c->fd, KILL_FAILURE2, sizeof(KILL_FAILURE2)-1 );
	/** Reset the application ID anyways. */
	c->application = 0;
    }
    /** Write the footer. */
    write( c->fd, KILL_FOOTER, sizeof(KILL_FOOTER)-1 );

    return c->context;
}

int context_license( struct c_stub *c ) {
    /** Print License and disclaimer. */
    int flic = open( MLVM_DATADIR "/LICENSE", O_RDONLY );
    if( flic < 0 ) {
	write( c->fd, LICENSE_NA, sizeof(LICENSE_NA)-1 );
    } else {
	char buf[255];
	int len = 0;
	while( (len = read(flic, buf, sizeof(buf))) > 0 ) {
	    write( c->fd, buf, len );
	}
	close(flic);
    }
    return c->context;
}
int context_disclaimer( struct c_stub *c ) {
    /** Print License and disclaimer. */
    int fdis = open( MLVM_DATADIR "/DISCLAIMER", O_RDONLY );
    if( fdis < 0 ) {
	write( c->fd, DISCLAIMER_NA, sizeof(DISCLAIMER_NA)-1 );
    } else {
	char buf[255];
	int len = 0;
	while( (len = read(fdis, buf, sizeof(buf))) > 0 ) {
	    write( c->fd, buf, len );
	}
	close(fdis);
    }
    return c->context;
}

char *get_filename(char *argv0) {
    char *p;

    if (argv0 == NULL)
	return "unknown";       /* XXX */
    p = strrchr(argv0, '/');
    if (p == NULL)
	p = argv0;
    else
	p++;
    return p;
}
