/** $RCSfile: imago_layer_marshaling.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_marshaling.h,v 1.3 2003/03/21 13:12:03 gautran Exp $ 
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

#ifndef __IMAGO_LAYER_MARSHALING_H_
#define __IMAGO_LAYER_MARSHALING_H_

void imago_layer_marshaling_init();
int imago_layer_marshaling_getopt( int, void * );
int imago_layer_marshaling_setopt( int, void * );
/** State machine ids. */
extern state_id_t imago_layer_marshaling_up_stateid;
extern state_id_t imago_layer_marshaling_down_stateid;
/** */

#endif
