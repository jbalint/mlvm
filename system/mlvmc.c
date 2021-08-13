/** $RCSfile: mlvmc.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: mlvmc.c,v 1.8 2003/03/25 16:19:58 james Exp $ 
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
/** This is the MLVMC main file. 
    The program opens a UNIX socket to the server MLVM that must be running on the same machine. 
    The UNIX port may be specified in the command line. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/un.h>

#include <string.h>
#include <errno.h>

#include "system.h"
#include "servconf.h"
#include "log.h"

#ifdef _MLVM_TMPDIR
# define MLVM_TMPDIR _MLVM_TMPDIR
#else
# define MLVM_TMPDIR "/tmp"
#endif

/** Server option structure. */
ServerOptions options;

int unix_fd = -1;

static void usage(void) {
    fprintf( stderr, MLVMC_NAME " version " MLVMC_VERSION "\n" );
    fprintf( stderr, "Usage: mlvmc [options]\n" );
    fprintf( stderr, "Options:\n" );
    fprintf( stderr, "  -f file    Server configuration file to read instead of default\n" );
    fprintf( stderr, "  -u port    Contact the server using the specific port (default: %d)\n", options.localport );
    fprintf( stderr, "  -v         Print version information and exit\n" );
    fprintf( stderr, "  -h         This help\n" );
    fprintf( stderr,  MLVM_COPYRIGHT "\n" );
    exit(1);
}

int main(int ac, char** av){
    /** Options argument variables. */
    extern char *optarg;
    extern int optind;
    int opt;
    struct sockaddr_un local_addr;
    fd_set  readfds;
    int stop = 0;
    char buf[1024];
    int len, l_port = -1;
    /** Suppress messages. */
    initialize_server_options(&options); 

    /** Parse command-line arguments. */
    while( (opt = getopt(ac, av, "f:u:hv?")) != -1 ) {
	switch (opt) {
	case 'u':
	    l_port = atoi(optarg);
	    if( l_port < 0 || l_port > 65535 ) {
		fprintf( stderr, "Bad port number.\n");
		exit(1);
	    }
	    break;
	case 'f': 
	    strncpy( options.config, optarg, sizeof(options.config) - 1 );
	    break;
	case 'v':
	  fprintf( stderr, MLVMC_NAME " version " MLVMC_VERSION "\n" );
	  exit(1);
	  break;
	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}
    }

    /** Read configuration file. */
    read_server_config(&options);

    /** Overwrite the UNIX port. */
    if( l_port >= 0 ) 
	options.localport = l_port;	
    /** Restore proper loging level. */
    level(options.log_level);

    /** Create the UNIX socket. */
    if( (unix_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0 ) {
	printf( "Unable to create a local UNIX socket.\n" );
	return 0;
    }

    /** Set the local server address. */
    snprintf( local_addr.sun_path, UNIX_PATH_MAX, "%s/mlvmd_%d", MLVM_TMPDIR, options.localport );
    local_addr.sun_family = PF_UNIX;

    /**Contact the server. */
    if( connect( unix_fd, (struct sockaddr*)&local_addr, sizeof(struct sockaddr_un) ) < 0 ) {
	close( unix_fd );
	unix_fd = -1;
	printf( "Unable to contact the server on local port [%d]\n", options.localport );
	return -1;
    }
    /** And we are off ! ! ! */
    printf( "Server contacted on local TCP port [%d]\n", options.localport );

    while( !stop ) {
	/** Clear the fdset. */
	FD_ZERO(&readfds);
	FD_SET( unix_fd, &readfds );
	FD_SET( fileno(stdin), &readfds );

	select( ((unix_fd < fileno(stdin))? fileno(stdin): unix_fd) + 1, &readfds, NULL, NULL, NULL );
	if( FD_ISSET(fileno(stdin), &readfds) ) {
	    char *ptr = fgets( buf, sizeof(buf), stdin ); 
	    if( ptr == NULL ) break;
	    /** Copy the line from the keyboard to the socket. */
	    len = strlen( ptr );
	    write( unix_fd, buf, len );
	} else if( FD_ISSET(unix_fd, &readfds) ) {
	    /** copy from the socket to the screen. */
	    len = read( unix_fd, buf, sizeof(buf) );
	    if( len <= 0 ) stop = 1;
	    else fwrite( buf, len, 1, stdout );
	    fflush(stdout);
	} else {
	    printf("Whatever...\n" );
	}
    }

    shutdown( unix_fd, SHUT_RDWR );
    close( unix_fd );
    return 0;
}
