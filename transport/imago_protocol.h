/** $RCSfile: imago_protocol.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_protocol.h,v 1.5 2003/03/21 19:39:02 gautran Exp $ 
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

/** Header required include. */
#include "include.h"
#include "imago_pdu.h"
#include "imago_icb.h"
#include "transport.h"
/** */

#ifndef __IMAGO_PROTOCOL_H_
#define __IMAGO_PROTOCOL_H_

extern const int SIGrestart;

/** This is the worker thread. */
#ifdef _REENTRANT
void *imago_protocol_thread( void * );
#else
void imago_protocol_thread( void );
#endif

/** This is the main entry point for this imago protocol stack. */
int imago_protocol( );
/** Duplicate in transport.h */

/** In a multi-threaded (aka: asynchronous) application, receiving ICB's is done
    through registration of one single handler. */
//typedef void (*imago_protocol_recv_t)( struct imago_control_block *, char *from );

/** This is the list of available layers. 
    Each entry in the table map to a specific layer. */
//extern struct imago_pdu *(*imago_layer_table[])( struct imago_pdu * );

/** This is the length (number of element) found in the layer table. */
//#define imago_layer_table_length (sizeof(imago_layer_table)/sizeof(struct imago_pdu *(*)(struct imago_pdu *)))


//void imago_protocol_safe_handler( int, void (*)(struct imago_pdu *) );
//void imago_protocol_init();
//int imago_protocol_send ( const char*, struct imago_control_block * );
/** To spawn some thread getting the job done. */
//int imago_protocol_spawn();

//#ifdef _REENTRANT
/** In a multi-threaded (aka: asynchronous) application, receiving ICB's is done
    through registration of one single handler. */
//imago_protocol_recv_t imago_protocol_register( imago_protocol_recv_t );
//#endif
/** */

#endif
