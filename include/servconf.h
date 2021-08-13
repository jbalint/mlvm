/** $RCSfile: servconf.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: servconf.h,v 1.7 2003/03/21 13:12:03 gautran Exp $ 
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

#ifndef __SERVCONF_H_
#define __SERVCONF_H_


#ifndef _MLVM_CONFIG_FILE
#define MLVM_CONFIG_FILE "/etc/mlvm/mlvm.conf"
#else
#define MLVM_CONFIG_FILE _MLVM_CONFIG_FILE
#endif


typedef struct {
    char config[FILENAME_MAX];
    char logfile[FILENAME_MAX];
    int  log_level;
    int  servport;
    int  localport;
    int  no_daemon_flag;
    int  max_server_threads;
    char hostname[MAX_DNS_NAME_SIZE];
    //    int  max_transport_threads;
} ServerOptions;


void initialize_server_options(ServerOptions *);
void read_server_config(ServerOptions *);
/** Server configuration options. */
extern ServerOptions options;

char *get_progname(char *);
#endif
