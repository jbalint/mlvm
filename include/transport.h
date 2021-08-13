/** $RCSfile: transport.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: transport.h,v 1.15 2003/03/24 14:07:48 gautran Exp $ 
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
/*	 	transport.h                     */
/************************************************/

#ifndef __TRANSPORT_H_
#define __TRANSPORT_H_

/** This is the main entry point for this imago protocol stack. */
void transport_init();
void transport_shutdown();
void transport_terminate();
int  transport_send ( const char*,  struct imago_control_block * );
void transport_signal();
void transport_signal_wait();
int  transport_run();
int  transport_run_signals();

/** To spawn some thread getting the job done. */
int transport_spawn();

/** Setting or getting options from the transport stack. */
int get_transportopt( int, int, void * );
int set_transportopt( int, int, void * );
#define LAYER_TRANSPORT     0
#define LAYER_CONNECTION    1
#define LAYER_ROUTING       2
#define LAYER_SECURITY      3
#define LAYER_ADARENDEZVOUS 4
#define LAYER_MARSHALING    5

#define T_MAX_THREAD        1

#define R_SERVER_NAME       1

#define C_SERVER_PORT       1
#define C_SERVER_TCP_PORT   2
#define C_SERVER_UDP_PORT   3
#define C_TIMEOUT_SEC       4


/** In a multi-threaded (aka: asynchronous) application, receiving ICB's is done
    through registration of one single handler. */
typedef void (*transport_recv_t)( const char *, struct imago_control_block * );
/** In a multi-threaded (aka: asynchronous) application, receiving ICB's is done
    through registration of one single handler. */
transport_recv_t transport_register_callback( transport_recv_t );

/** Routine used to retreive the corrent machine name. */
int get_imago_localhost( char *, const int );

/** Loopback interface. */
#ifndef _IMAGO_LOOPBACK_INTERFACE
#define LOOPBACK_INTERFACE "loopback"
#else
#define LOOPBACK_INTERFACE _IMAGO_LOOPBACK_INTERFACE
#endif

/** General hash function. */
int strhash_np(const char*, const int);

#endif
