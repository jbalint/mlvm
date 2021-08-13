/** $RCSfile: imago_layer_ADArendezvous.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_ADArendezvous.h,v 1.9 2003/03/24 14:07:48 gautran Exp $ 
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

#ifndef __IMAGO_LAYER_ADARENDEZVOUS_H_
#define __IMAGO_LAYER_ADARENDEZVOUS_H_

void imago_layer_ADArendezvous_init();
void imago_layer_ADArendezvous_shutdown();
int imago_layer_ADArendezvous_getopt( int, void * );
int imago_layer_ADArendezvous_setopt( int, void * );
/** State machine ids. */
extern state_id_t imago_layer_ADArendezvous_up_stateid;
extern state_id_t imago_layer_ADArendezvous_down_stateid;
/** */

/** Used to prefix the name of all save PDUs. */
#define ADA_PREFIX "ada"

/** ADA PDU types. */
#define ADA_TYPE_REQUEST 0x0100
#define ADA_TYPE_ACCEPT  0x0200
#define ADA_TYPE_ACK     0x0400
#define ADA_TYPE_NACK    0x0800
#define ADA_TYPE_RETRY   0x0001


/** Timeout used when a REQUEST packet does not get ACCEPT 
    and when an ACCEPT packet does not get ACK. */
#define ADA_ACCEPT_TIMEOUT 3 MIN
#define ADA_ACK_TIMEOUT 1 MIN

#define ADA_MAX_RETRY 3

#endif

