/** $RCSfile: log.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: log.c,v 1.8 2003/03/24 14:07:48 gautran Exp $ 
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
#include <stdarg.h>
#include <syslog.h>

#include "log.h"

static int plevel = LOG_NOTICE;
static int psyslog = 0;

int vlog_syslog( register int prio, const char *pre, const char *fmt, va_list ap ) {
    if( pre != NULL && plevel >= LOG_DEBUG )
	syslog( prio, "%s", pre );
  
    vsyslog( prio, fmt, ap);
    return 1;
}

int vlog( register int prio, const char *pre, const char *fmt, va_list ap ) {
    register FILE *fp;
    register int count = 0;
    /** The priority is too low. */
    if( prio > plevel ) return 0;

    if( psyslog ) 
	return vlog_syslog( prio, pre, fmt, ap );

    /** DEBUG, INFO, NOTICE are logged on stdout. 
	Higher errors are logged on stderr. */
    fp = ( prio > LOG_WARNING ) ? stdout: stderr;

    if( pre != NULL && plevel >= LOG_DEBUG )
	count = fprintf( fp, "%s", pre );
  
    count += vfprintf( fp, fmt, ap);
    fflush( fp );
    return count;
}

void level( int prio ) {
    plevel = prio;
}

void use_syslog( int b ) {
    psyslog = b;
    if( psyslog )
	openlog(NULL, LOG_PID, LOG_DAEMON);
}

int g_log(int prio, const char *fmt, ...) {
    int ret;
    va_list ap;  
    va_start(ap, fmt);
    ret = vlog( prio, "", fmt, ap);
    va_end(ap);
    return ret;
}

int g_critical( const char *fmt, ...) {
    int ret;
    va_list ap;  
    va_start(ap, fmt);
    ret = vlog( LOG_CRIT, "CRITICAL: ", fmt, ap);
    va_end(ap);
    return ret;
}
int g_error( const char *fmt, ...) {
    int ret;
    va_list ap;  
    va_start(ap, fmt);
    ret = vlog( LOG_ERR, "ERROR: ", fmt, ap);
    va_end(ap);
    return ret;
}
int g_warning( const char *fmt, ...) {
    int ret;
    va_list ap;  
    va_start(ap, fmt);
    ret = vlog( LOG_WARNING, "WARNING: ", fmt, ap);
    va_end(ap);
    return ret;
}
int g_info( const char *fmt, ...) {
    int ret;
    va_list ap;  
    va_start(ap, fmt);
    ret = vlog( LOG_INFO, "", fmt, ap);
    va_end(ap);
    return ret;
}
int g_notice( const char *fmt, ...) {
    int ret;
    va_list ap;  
    va_start(ap, fmt);
    ret = vlog( LOG_NOTICE, "", fmt, ap);
    va_end(ap);
    return ret;
}
int g_debug( const char *fmt, ...) {
    int ret;
    va_list ap;  
    va_start(ap, fmt);
    ret = vlog( LOG_DEBUG, "DEBUG: ", fmt, ap);
    va_end(ap);
    return ret;
}

