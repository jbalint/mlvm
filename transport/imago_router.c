/** $RCSfile: imago_router.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_router.c,v 1.7 2003/03/21 13:12:03 gautran Exp $ 
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
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "queue.h"
#include "imago_signal.h"
#include "imago_protocol.h"

/** Saved arguments to main(). */
char **saved_argv;
int saved_argc;

#ifdef HAVE___PROGNAME
extern char *__progname;
#else
char *__progname;
#endif
char *get_progname(char *);
static void usage(void);


void SIG_handler(int sig, void *arg) {
#ifdef _REENTRANT
    extern int pthread_count;
    extern pthread_mutex_t pthread_count_mutex;
#endif
    debug("%s(%p)\n", __PRETTY_FUNCTION__, arg );
    /** Initiate the stack shutdown. */
    transport_shutdown();
#ifdef _REENTRANT
    pthread_mutex_lock(&pthread_count_mutex);
    pthread_count--;
    pthread_mutex_unlock(&pthread_count_mutex);
#endif
    transport_terminate();
}

int main_UNUSED( int argc, char **argv ) {
    /** Options argument variables. */
    extern char *optarg;
    extern int optind;
    int opt;
    int log_level = LOG_WARNING, servport = IMAGO_SERVER_PORT, tnum = IMAGO_PROTOCOL_MAX_THREAD, no_daemon_flag = 0;

#ifdef MEMWATCH
    /* Collect stats on a line number basis */
    mwStatistics( MW_STAT_LINE );

    mwNoMansLand( MW_NML_ALL );

    mwAutoCheck( 1 );
#endif

    /** Retreive the program name. */
    __progname = get_progname(argv[0]);

    /** Save argv. */
    saved_argc = argc;
    saved_argv = argv;

    /** Parse command-line arguments 2nd time. */
    while( (opt = getopt(argc, argv, "t:f:l:p:u:i:Dqhd")) != -1 ) {
	switch (opt) {
	case 'd':
	    log_level = LOG_DEBUG;
	    break;
	case 'l':
	    log_level = atoi(optarg);
	    break;
	case 't':
	    tnum = atoi(optarg);
	    break;
	case 'D':
	    no_daemon_flag = 1;
	    break;
	case 'q':
	    log_level = LOG_WARNING;
	    break;
	case 'p':
	    servport = atoi(optarg);
	    if( servport < 0 || servport > 65535 ) {
		critical( "Bad port number.\n");
		exit(1);
	    }
	    break;
	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}
    }

    if( no_daemon_flag == 0 ) {
	pid_t pid = fork();
	if( pid == 0 ) {
	    /** The child. */
	    /** Close up the stdin/stdout. 
		or redirect them into a log file. */
	    setsid();  /** Create a process session with this guy in it. */
	    setpgrp(); /** Make the process be a process group leader */
	    chdir("/");  /** Move the the root directory so we know where we are. */
	    umask(0); /** control our umask. */
	    /** Close all standard streams. */
	    fclose(stdin);
	    fclose(stdout);
	    fclose(stderr);
	    /** Reopen all stream to the /dev/null device. */
	    stdin  = fopen( "/dev/null", "r" );
	    stdout = fopen( "/dev/null", "w" );
	    stderr = fopen( "/dev/null", "w" );
	    /** Set the logger to use syslog instead of the std... streams. */
	    use_syslog( 1 );
	} else if( pid > 0 ) {
	    /** The parent. */
	    notice("Imago router daemon started:\n");
	    exit(pid);
	} else {
	    /** Error. */
	    critical( "Unable to fork daemon process.\n" );
	    exit(-1);
	}
    } else 
	notice("Imago router started.\n");

    level(log_level);

    /** Initialize the transport stack options (pre init). */
    set_transportopt( LAYER_CONNECTION, C_SERVER_PORT, &servport );
    set_transportopt( LAYER_TRANSPORT, T_MAX_THREAD, &tnum );

    /** Initialize the transport stack. */
    transport_init();

    /** We are going to reuse/override the transport signals. */
    /** Set the signal handler for the SIGINT. */
    imago_signal( SIGINT, SIG_handler, NULL );

    /** Set the signal handler for the SIGTERM. */
    imago_signal( SIGTERM, SIG_handler, NULL );


    /** This is the main function for the process control. */
#ifdef _REENTRANT
    /** Spwan thread to work on the requests. */
    transport_spawn();
    
    /** Even the master thread must be detached... */
    pthread_detach( pthread_self() );
    
    /** The very first thread is US and must be the IO master ! ! */
    imago_protocol_thread( (void *)1 );

#else
    /** In no multithreaded, we are alone to do the job. */
    imago_protocol_thread( );
#endif
    return 0;
}

char *get_progname(char *argv0)
{
#ifdef HAVE___PROGNAME
        extern char *__progname;

        return __progname;
#else
        char *p;

        if (argv0 == NULL)
                return "unknown";       /* XXX */
        p = strrchr(argv0, '/');
        if (p == NULL)
                p = argv0;
        else
                p++;
        return p;
#endif
}

static void usage(void) {
    critical( "Imago router version %s\n", IMAGO_R_VERSION );
    critical( "Usage: %s [options]\n", __progname );
    critical( "Options:\n" );
    critical( "  -d         Debugging mode\n" );
    critical( "  -D         Do not fork into daemon mode\n" );
    critical( "  -h         This help\n" );
    critical( "  -l level   Logging level(High=7, None=0)\n" );
    critical( "  -p port    Listen on the specified port (default: 2223)\n" );
    critical( "  -q         Quiet (no logging)\n" );
    critical( "  -t num     Number of system threads (default: 10)\n" );
    critical( "Copyright 2003: Guillaume Autran & Dr. Xining Li\n  University of Guelph, Ontario, Canada\n" );
    exit(1);
}
