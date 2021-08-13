/** $RCSfile: mlvm.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: mlvm.c,v 1.34 2003/03/24 14:07:48 gautran Exp $ 
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
/*	 	  mlvm.c                        */
/************************************************/
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "system.h"
#include "servconf.h"
#include "opcode.h"
/** Include the transport stack. */
#include "transport.h"
#include "imago_signal.h"
#include "s_stub.h"

extern void msg_init();
extern void msg_cleanup();

/************************************************/
/* The main program to start a mlvm server.     */
/* It will spawn system threads and initialize  */
/* system queues as well as mutex variables.    */
/************************************************/

/** Inform all threads to terminate what they are doing. */
int mlvm_stop = 0;
/** Number of threads in the system. */
int thread_count = 0;
/** Thread Waiting For Jobs counter (ie:number of thread executing the "system_wait()"). */
int twfj = 0;
/** Number of jobs to carry out in the system. */
int jobs = 0;
pthread_mutex_t job_n_twfj_mutex = PTHREAD_MUTEX_INITIALIZER;
/** Thread pool locking variables. */
int system_count = 0;
pthread_mutex_t system_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  system_cond = PTHREAD_COND_INITIALIZER;	
/** */
/** The application ID is derived from a generation ID and an application specific ID. */
pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;
short gen_id = 0;//time(NULL) & 0xffff;
short app_id = 0;
/** */


/** Saved arguments to main(). */
char **saved_argv;
int saved_argc;

/** Server option structure. */
ServerOptions options;

#ifdef HAVE___PROGNAME
extern char *__progname;
#else
char *__progname;
#endif


struct icb_queue_type queues[12] = {
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
    {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER},
};

log_queue*	free_logs = (log_queue*) &queues[0];
log_queue*	live_logs = (log_queue*) &queues[1];

icb_queue*	free_queue        = (icb_queue*) &queues[2];
icb_queue*	creator_queue     = (icb_queue*) &queues[3];
icb_queue*	memory_queue      = (icb_queue*) &queues[4];
icb_queue*	engine_queue      = (icb_queue*) &queues[5];
icb_queue*	out_queue         = (icb_queue*) &queues[6];
icb_queue*	in_queue          = (icb_queue*) &queues[7];
icb_queue*	timer_queue       = (icb_queue*) &queues[8];
icb_queue*	security_queue    = (icb_queue*) &queues[9];
icb_queue*	waiting_mem_queue = (icb_queue*) &queues[10];
icb_queue*      waiting_msger_queue = (icb_queue*) &queues[11];



// error_msg must match with the typedef of error_code

char* error_msg[] = {"succeeds",
		     "no solution",
		     "memory exhausted",
		     "too many imagoes (icb exhausted)",
		     "unknown imago type",
		     "unknown imago file",
		     "unknown opcode",
		     "illegal imago thread",
		     "illegal access",
		     "illegal messenger",
		     "illegal imago creation",
		     "illegal host name",
		     "illegal builtin usage"
};

/** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
/** !! DO NOT CHANGE THE ORDER OF THE FOLLOWING TABLE !! */
/** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
/** Imago life cycle entry table. */
int (*entry_tbl[])() = {
    transport_run_signals,
    transport_run,
    imago_out,
    imago_timer,
    imago_creator,
    imago_in,
    imago_memory,
    imago_scheduler,
    NULL
};
/** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
/** !! DO NOT CHANGE THE ORDER OF THE ABOVE TABLE     !! */
/** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

int dispose_of( ICBPtr kip ) {
    debug("Disposing of %s(%d):%p\n", kip->name, kip->status, kip );
    /** Make sure we are not passed an outuf data icb. */
    if( kip->status == IMAGO_INVALID ) {
    }
    switch( kip->status ) {
    case IMAGO_EXPANSION:
    case IMAGO_COLLECTION:
	/** He is waiting for memory... */
	/** Let us kill that poor guy... */
	kip->status = IMAGO_TERMINATE;
	/** WARNING, we cannot remove him from the memory queue. 
	    But, do not worry, it will come out eventually, someday... :) */
	break;
    case IMAGO_ERROR:     
    case IMAGO_MOVE_OUT:  
    case IMAGO_DISPOSE:   
    case IMAGO_TERMINATE: 
	/** As good as dead. */
	break;

    case IMAGO_MOVE_IN:
    case IMAGO_CLONING:
    case IMAGO_DISPATCHING:  
    case IMAGO_CREATION:    
    case IMAGO_SYS_CREATION:
	/** Will come out eventually... */
	kip->status = IMAGO_TERMINATE;
	break;

    case IMAGO_ATTACHING:
	/** For the ATTACHING case, the ICB belongs to a messenger. 
	    This messenger been located in some IMAGO messenger queue, 
	    it will eventually come out someday, so we do not touch him... */
	break;
      
    case IMAGO_WAITING:
	/** Blocked waiting for some messengers to arrive. */
	/** Let us kill that poor guy... */
	kip->status = IMAGO_TERMINATE;
	icb_put_signal( engine_queue, kip );
	break;
	
    case IMAGO_TIMER:
      /** An imago inside the timer queue will not be killed right away but as soon as possible... */
      memset( &kip->dday, 0, sizeof(kip->dday) ); /** We make him die a bit sooner... */
      	kip->status = IMAGO_TERMINATE;
	break;
    case IMAGO_READY:
	/** Let us kill that poor guy... */
	kip->status = IMAGO_TERMINATE;
	break;
    case IMAGO_INVALID:
	notice("Trying to dispose of a dead imago.\n" );
	return -1;
    default: 
	warning("%s: Invalid imago status (%d).\n", __PRETTY_FUNCTION__, kip->status );
	return -1;
    }

    return 1;
}

/** MLVM server start begins. */

void system_error(char* who, error_code e){
    critical("*** %s : %s\n", who, error_msg[e]);
}

/** Terminate all imago waiting in queues. */
void system_flush() {
    LOGPtr log;
    ICBPtr lip;
    info("Flushing Imago...\n");
    /** All imago currently within the system must have an entry in the log.
	The guys that need to be flushed are the blocked ones... */
 retry:
    lip = NULL;
    log_lock();
    for( log = live_logs->head; log; log = log->next ) {
	if( log->status == LOG_BLOCKED && log->ip != NULL ) {
	    lip = log->ip;
	    log->status = LOG_DISPOSED;
	    break;
	}
    }
    log_unlock();
    if( lip ) {
	icb_lock( lip );
	if( lip->status != IMAGO_INVALID ) {
	    lip->status = IMAGO_TERMINATE;
	    icb_put(engine_queue, lip);
	}
	icb_unlock( lip );
	goto retry;
    }
}


void system_shutdown(int sig, void *arg) {
    debug("%s(%p)\n", __PRETTY_FUNCTION__, arg );
    if( mlvm_stop ) return; /** Been here already... */
    /** First thing to do, indicate the shutdown. */
    mlvm_stop = 1;
    /** All imago waiting in the numerous queue should be flushed. */
    system_flush();
    /** Initiate the stack shutdown. */
    transport_shutdown();

}

void system_terminate(int sig, void *arg) {
    debug("%s(%p)\n", __PRETTY_FUNCTION__, arg );
    /** For now, system terminate is the same as a gracefull shutdown. */
    system_shutdown( sig, arg );
}

void system_init(){
    log_init();
    icb_init();
    con_nil = make_con(ASCII_SIZE);
    msg_init();

    /** Initialize the transport stack options (pre init). */
    set_transportopt( LAYER_CONNECTION, C_SERVER_PORT, &options.servport );
    set_transportopt( LAYER_ROUTING, R_SERVER_NAME, &options.hostname );
    //    set_transportopt( LAYER_TRANSPORT,  T_MAX_THREAD, &options.max_transport_threads );
    /** Initialize the transport protocol stack. */
    transport_init();

    /** Initialize the client stub 
	(must be after the transport stack init but before the thread spwaning). */
    s_stub_init( options.localport );
 
    /** Spwan new transport threads. */
    //    transport_spawn();
    /** */
    /** The transport protocol requires us to register a callback function
	for when something is received... */
    transport_register_callback( imago_in_cb );
    /** */
    /** We are going to reuse/override the transport signals. */
    /** Set the signal handler for the SIGINT. */
    imago_signal( SIGINT, system_terminate, NULL );

    /** Set the signal handler for the SIGTERM. */
    imago_signal( SIGTERM, system_shutdown, NULL );

    /** Initialize the generation id. */
    gen_id = time(NULL) & 0xffff;

    // to be expanded
}

/** Main thread functions. */
void *pthread_main_np( void *arg ) {
    int i;
    int(**entry)() = (int(**)())arg;

    /** Register ourselves as a active working thread. */
    pthread_mutex_lock(&system_lock);
    thread_count++;
    pthread_mutex_unlock(&system_lock);


    /** The argument 'arg' is a pointer to a table of pointer to functions.
	Those functions are the main entry point for an Imago. One can think
	of it as the life cycle of the Imago. You can find life states like
	'creation', 'move in', 'move out', 'memory mgmt', and the 'scheduler'.
	The table is ordered so starvation does not occur. For example, the 
	'move out' always comes first because moving an Imago out will release
	resources and decrease the load of the machine... The last element is
	the scheduler where all thread will go if nothing else is to be done 
	in the earlier entry.
	The table is NULL terminated so we do not need to know the size of it...
    */
    while( 1 ) {
	for( i = 0; entry[i] != NULL; ) {
	    if( entry[i]() != 0 )
		i = 0;
	    else
		i++;
	}
	/** Absolutely nothing to do... 
	    if the server is terminating, just bail out. */
	if( mlvm_stop ) break;
	/** Otherwise, wait for anything to happen. */
	system_wait();
    }

    /** Signal the main thread that we past out... */
    pthread_mutex_lock(&system_lock);
    thread_count--;
    if( thread_count == 0 )
	pthread_cond_signal( &system_cond ); 
    pthread_mutex_unlock(&system_lock);
    /** */

    /** The server is exiting... */
    return NULL;
}
/** */

static void usage(void) {
    critical( MLVM_NAME " version " MLVM_VERSION "\n" );
    critical( "Usage: %s [options]\n", __progname );
    critical( "Options:\n" );
    critical( "  -d         Debugging mode\n" );
    critical( "  -D         Do not fork into daemon mode\n" );
    critical( "  -f file    Server configuration file to read instead of default\n" );
    critical( "  -h         This help\n" );
    critical( "  -i file    Start an Imago program (Obsolete, see mlvmc)\n" );
    critical( "  -l level   Logging level(High=7, None=0)\n" );
    critical( "  -p port    Listen on the specified port (default: 2223)\n" );
    critical( "  -q         Quiet (no logging)\n" );
    critical( "  -t num     Number of system threads (default: 10)\n" );
    critical( "  -u port    The local port the server stub listens to (default: 2223)\n" );
    critical( "  -v         Print version information and exit\n" );
    critical( MLVM_COPYRIGHT );
    exit(1);
}

int main(int ac, char** av){
    /** Options argument variables. */
    extern char *optarg;
    extern int optind;
    int opt;

    /** pthread related variables. */
    pthread_t ptid;
    pthread_attr_t attr;
    sigset_t sigs_to_block;
    /** Imago variables. */
    ICBPtr ip;
    /** List of filenames to start Imagoes from. */
    char *imagoes[20];
    /** General variables. */
    int i;
    FILE *std;
#ifdef MEMWATCH
    /* Collect stats on a line number basis */
    mwStatistics( MW_STAT_LINE );

    mwNoMansLand( MW_NML_ALL );

    mwAutoCheck( 1 );
#endif


    /** Clean up a bit. */
    memset( imagoes, 0, sizeof(imagoes) );

    /** Retreive the program name. */
    __progname = get_progname(av[0]);

    /** Save argv. */
    saved_argc = ac;
    saved_argv = av;

    initialize_server_options(&options);
    
    /** High logging level for startup. */
    level(LOG_CRIT);

    /** Parse command-line arguments to get the config file name. */
    while( (opt = getopt(ac, av, "t:f:l:p:u:i:Dqhdv?")) != -1 ) {
	switch (opt) {
	case 'f': 
	    strncpy( options.config, optarg, sizeof(options.config) - 1 );
	    break;
	case 'v':
	  critical( MLVM_NAME " version " MLVM_EXT_VERSION "\n" );
	  exit(1);
	  break;
	case 'h':
	case '?':
	    usage();
	    break;
	default:
	    break;
	}
    }
    /** Read configuration file. */
    read_server_config(&options);

    /** Restore proper logging level. */
    level(options.log_level);

    /** Reset the optind to the begining and restart the parsing. */
    optind = 1;

    /** Parse command-line arguments 2nd time. */
    while( (opt = getopt(ac, av, "t:f:l:p:u:i:Dqd")) != -1 ) {
	switch (opt) {
	case 'd':
	    options.log_level = LOG_DEBUG;
	    break;
	case 'l':
	    options.log_level = atoi(optarg);
	    break;
	case 't':
	    options.max_server_threads = atoi(optarg);
	    break;
	case 'D':
	    options.no_daemon_flag = 1;
	    break;
	case 'q':
	    options.log_level = LOG_WARNING;
	    break;
	case 'p':
	    options.servport = atoi(optarg);
	    if( options.servport < 0 || options.servport > 65535 ) {
		critical( "Bad port number.\n");
		exit(1);
	    }
	    break;
	case 'u':
	    options.localport = atoi(optarg);
	    if( options.localport < 0 || options.localport > 65535 ) {
		critical( "Bad port number.\n");
		exit(1);
	    }
	    break;
	case 'i':
	    /** If the command line supplied some file as argument, 
		it is a new imago to start. */
	    for( i = 0; i < (sizeof(imagoes)/sizeof(char *)); i++ ) {
		if( imagoes[i] == NULL ) {
		    imagoes[i] = optarg;
		    break;
		}
	    }
	    if( i >= (sizeof(imagoes)/sizeof(char *)) ) {
		critical( "Too many file to load (%d max).\n", (sizeof(imagoes)/sizeof(char *)) );
		exit(1);
	    }
	    options.no_daemon_flag = 1;
	    break;
	case 'f': 
	    strncpy( options.config, optarg, sizeof(options.config) - 1 );
	    break;
	default:
	    usage();
	    break;
	}
    }

    if( options.no_daemon_flag == 0 ) {
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
	    int status;
	    /** Sleeping for two second should be long enough for the child to successfully start or dye. */
	    sleep(2); 
	    if( waitpid(pid, &status, WNOHANG) != 0 ){
		notice("Starting MLVM Daemon failed.\n");
		exit(-1);
	    }
	    notice("MLVM Daemon started.\n");
	    exit(0);
	} else {
	    /** Error. */
	    critical( "Unable to fork daemon process.\n" );
	    exit(-1);
	}
    } else 
	notice("MLVM started.\n");

    level(options.log_level);

    /** If a output file name has been provided, we redirect the stdout/stderr into is so
	we can look at whatever the imago have been up to. */
    if( strlen( options.logfile ) ) {
	/** Close stdout and stderr standard streams. */
	fclose(stdout);
	fclose(stderr);
	/** Reopen the streams to the /dev/null device. */
	if( (std = fopen( options.logfile, "w" )) == NULL ) {
	    warning("Unable to open output stream for writing to file '%s' (outputs will be discarded).\n",
		    options.logfile );
	} else {
	    stdout = std;
	    if( (std = fopen( options.logfile, "w" )) == NULL ) {
		warning("Unable to open error stream for writing to file '%s' (error outputs will be discarded).\n",
			options.logfile );
	    } else {
		stderr = std;
	    }
	}
    }
    /** System initialization. */
    system_init();

    /** Initialize the newly created thread attribute. */
    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

    /** Block all signals */
    sigfillset( &sigs_to_block );
    pthread_sigmask( SIG_BLOCK, &sigs_to_block, NULL );

    info( "Starting Imago server threads...\n" );
    /* We will now create MAX_SYSTEM_THREADS-1 system threads. */
    for( i = 1; i < options.max_server_threads; i++ ) 
	pthread_create( &ptid, &attr, pthread_main_np, entry_tbl );

    /** Don't need this anymore... */
    pthread_attr_destroy( &attr );

    /** Detach ourselves. */
    pthread_detach( pthread_self() );

    /** Load the file provided in the argument list. */
    for( i = 0; i < (sizeof(imagoes)/sizeof(char *)); i++ ) {
	if( imagoes[i] == NULL ) break;
	notice( "Loading imago file: '%s'...\n", imagoes[i] );
	ip = icb_new();
	strcpy(ip->name, imagoes[i]);
	ip->status = IMAGO_CREATION;
	ip->call_seq = 0;
	ip->start_num = 0;
	ip->call_count = 0;
	ip->mid_call = 0;
	icb_put_signal(creator_queue, ip);
    }
    info( "MLVM Server is running.\n" );


    /** Now the main thread will be used the same way as the other ones. */
    pthread_main_np( entry_tbl );

    /** Continue the proper server shutdown. */
    /** Because we are the main thread, we need to wait a little otherwise we will abort
	all other threads. */
    pthread_mutex_lock(&system_lock);
    while( thread_count != 0 )
	pthread_cond_wait( &system_cond, &system_lock ); 
    pthread_mutex_unlock(&system_lock);

    /** Close all CLI clients. */
    s_stub_cleanup();
   
    /** Terminate the transport stack. */
    transport_terminate();

    /** At this point, all ICB should be freeded, as well as the logs. */
    msg_cleanup();
    icb_cleanup();
    log_cleanup();
    /** At this point, all thread have been terminated. */
    notice("MLVM Server exit.\n");
    return(1);
}

int get_unique_appid() {
    int id = gen_id;
    id <<= sizeof(short)*8;
    pthread_mutex_lock(&id_lock);
    id += app_id++;
    pthread_mutex_unlock(&id_lock);
    return id;
}


/** THE END */

int pthread_mutex_lock_np( void *L, const char *fn, const int line ) {
    fprintf( stderr, "pthread_mutex_lock(%p) at %s: %d\n", (L), fn, line);
#undef  pthread_mutex_lock
    return pthread_mutex_lock(L);
}
int pthread_mutex_unlock_np( void *L, const char *fn, const int line ) {
    fprintf( stderr, "pthread_mutex_unlock(%p) at %s: %d\n", (L), fn, line );
#undef  pthread_mutex_unlock
    return pthread_mutex_unlock(L);
}
