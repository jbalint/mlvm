/** $RCSfile: servconf.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: servconf.c,v 1.10 2003/03/21 13:12:03 gautran Exp $ 
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
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "system.h"
#include "servconf.h"
#include "log.h"

#define INTEGER 1
#define STRING 2
#define YESNO 3

struct {
    const char *name;
    int offset;
    int type;
    int lo;
    int hi;
} options_table[] = {
    //{ "MaxTransportThread", (int)&((ServerOptions *)0)->max_transport_threads, INTEGER, 1, 256               },
    { "Hostname",           (int)&((ServerOptions *)0)->hostname,              STRING,  0, MAX_DNS_NAME_SIZE },
    { "LocalStubPort",      (int)&((ServerOptions *)0)->localport,             INTEGER, 0, 65535             },
    { "LogLevel",           (int)&((ServerOptions *)0)->log_level,             INTEGER, 0, 7                 },
    { "MaxSystemThread",    (int)&((ServerOptions *)0)->max_server_threads,    INTEGER, 1, 256               },
    { "OutputStreamfile",   (int)&((ServerOptions *)0)->logfile,               STRING,  0, FILENAME_MAX      },
    { "ServerPort",         (int)&((ServerOptions *)0)->servport,              INTEGER, 0, 65535             },
    { "StartAsDaemon",      (int)&((ServerOptions *)0)->no_daemon_flag,        YESNO,   0, 1                 },
    /** Must be last entry in the configuration table. */
    { NULL,                 0,                                                 0,       0, 0                 }
};


void initialize_server_options( ServerOptions *options ) {
  memset( options, 0, sizeof(ServerOptions) );

  strcpy( options->config, MLVM_CONFIG_FILE );
  options->log_level = LOG_NOTICE;
  options->servport = 2223;
  options->localport = 2223;
  options->no_daemon_flag = 0;
  options->max_server_threads = 10;
  //  options->max_transport_threads = 5;

}
long int gethostid_cache(void) {
  static long int hostid = 0;
  if( !hostid ) hostid = gethostid();
  return hostid;
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
int readline( FILE *fp, char *buf ) {
    static char cache[1024];
    static int  begin = 0;
    static int  end = 0;
    int i;

    if( begin >= end ) {
	memset( cache, 0, sizeof(cache) );
	begin = 0;
	end = fread( cache, 1, sizeof(cache)-1, fp ); 
	if( end <= 0 ) {
	    /** Most likely, we got the end-of-file.
		pretend the new line. */
	    buf[0] = '\n';
	    buf[1] = 0x00;
	    return 0;
	}
    }
    /** Remove leading spaces. */
    while( cache[begin] == ' ' || cache[begin] == '\t' )
	begin++;


    for( i = 0; begin < end; i++ ) {
	buf[i] = cache[begin++];
	if( buf[i] == '\n' ) {
	    buf[i+1] = 0x00;
	    return i+1;
	}
    }
    /** We did not find a newline, recursively call ourself. */
    return (i + readline(fp, &buf[i]) );
}

void read_server_config( ServerOptions *options ) {
    FILE *fp;
    struct stat statbuf;
    char buf[256];
    int line = 0, i, optv;
    char *argp;
   
    /** Open configuration file. */
    if( (fp = fopen( options->config, "r" )) == NULL ) {
	warning( "Unable to open configuration file for reading ('%s').\n", options->config );
	return;
    }
    if( fstat(fileno(fp), &statbuf) < 0 || !S_ISREG(statbuf.st_mode) ) {
	critical( "Configuration file name provided is not a file ('%s').\n", options->config );
	exit(-1);
	return;
    }
    notice("Reading configuration file: '%s'\n", options->config );

    while( readline(fp, buf) ) {
	line++;
	if( buf[0] =='#' || buf[0] ==';' || buf[0] == '\n' ) {
	    /** Comment lines and blank lines are ignored. */
	    continue;
	}
	/** Remove the '\n' at the end of the line. */
	for( i = 0; buf[i] != '\n' && i < sizeof(buf); i++ );
	buf[i] = 0x00;
	/** */
	for( i = 0; options_table[i].name; i++ ) {
	    optv = strlen(options_table[i].name);
	    if( strncmp( buf, options_table[i].name, optv ) == 0 ) {
		argp = &buf[optv];
		/** Skip the blanks and '=' sign, to the begining of the argument. */
		while( *argp == ' ' || *argp == '\t' || *argp == '=' ) argp++;
		switch( options_table[i].type ) {
		case INTEGER:
		    {
			optv = atoi( argp );
			if( optv < options_table[i].lo || optv > options_table[i].hi ) {
			    critical( "Invalid range for option %s at line %d\n",
				      options_table[i].name, line );
			    exit(-1);
			}
			*(int *)(((char *)options) + options_table[i].offset) = optv;
		    }
		    break;
		case STRING:
		    strncpy( (((char *)options) + options_table[i].offset), argp, options_table[i].hi-1 );
		    break;
		case YESNO:
		    if( strncmp( argp, "yes", 3 ) == 0 ||
			strncmp( argp, "true", 4 ) == 0 || 
			argp[0] == '1') {
			*(int *)(((char *)options) + options_table[i].offset) = options_table[i].lo;
		    } else {
			*(int *)(((char *)options) + options_table[i].offset) = options_table[i].hi;
		    }
		    break;
		default:
		    /** Programmer issue... */
		    break;
		}
		break;
	    }
	}
	if( !options_table[i].name ) {
	    /** Unknown directive. */
	    critical( "Unknown configuration option on line %d.\n", line);
	    exit(-1);
	}
    }
    fclose(fp);
}
