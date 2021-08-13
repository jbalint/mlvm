/** $RCSfile: imago_layer_security.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_security.h,v 1.5 2003/03/21 13:12:03 gautran Exp $ 
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

#ifndef __IMAGO_LAYER_SECURITY_H_
#define __IMAGO_LAYER_SECURITY_H_

void imago_layer_security_init();
int imago_layer_security_getopt( int, void * );
int imago_layer_security_setopt( int, void * );

/** State machine ids. */
extern state_id_t imago_layer_security_up_stateid;
extern state_id_t imago_layer_security_down_stateid;
/** */

/** Used to prefix the name of all save PDUs. */
#define SEC_PREFIX "sec"

/** How big should be our session key. Must be 32 bytes. */
#define SEC_SESSION_KEY_LENGTH 32

/** Types of SICP PDUs. */
#define SEC_TYPE_REQUEST  0x01
#define SEC_TYPE_SIGNED   0x02
#define SEC_TYPE_X4       0x04
#define SEC_TYPE_X8       0x08
#define SEC_TYPE_RSA_KEY  0x10
#define SEC_TYPE_DSA_KEY  0x20
#define SEC_TYPE_4X       0x40
#define SEC_TYPE_ENABLED  0x80


#endif
