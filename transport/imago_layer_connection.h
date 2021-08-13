/** $RCSfile: imago_layer_connection.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_connection.h,v 1.11 2003/03/21 13:12:03 gautran Exp $ 
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

#ifndef __IMAGO_LAYER_CONNECTION_H_
#define __IMAGO_LAYER_CONNECTION_H_

void imago_layer_connection_init();
void imago_layer_connection_shutdown();
void imago_layer_connection_terminate();
int  imago_layer_connection_getopt( int, void * );
int  imago_layer_connection_setopt( int, void * );

/** State machine ids. */
extern state_id_t imago_layer_connection_up_stateid;
extern state_id_t imago_layer_connection_down_stateid;
/** */

/** Maximum size the UDP layer may handle at once. */
#define UDP_MTU 1024

/** Magic number (32 bits). */
#define MAGIC_NUMBER 0x01234567

#endif
