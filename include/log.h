/** $RCSfile: log.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: log.h,v 1.4 2003/03/24 14:07:48 gautran Exp $ 
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


#ifndef __LOG_H_
#define __LOG_H_

int g_log( int, const char *, ...);
int g_critical( const char *, ...);
int g_error( const char *, ...);
int g_warning( const char *, ...);
int g_info( const char *, ...);
int g_notice( const char *, ...);
int g_debug( const char *, ...);

void level( int );
void use_syslog( int );


#define log(...)      g_log(__VA_ARGS__)
#define critical(...) g_critical(__VA_ARGS__)
#define error(...)    g_error(__VA_ARGS__)
#define warning(...)  g_warning(__VA_ARGS__)
#define info(...)     g_info(__VA_ARGS__)
#define notice(...)   g_notice(__VA_ARGS__)
#define debug(...)    g_debug(__VA_ARGS__)

#endif
