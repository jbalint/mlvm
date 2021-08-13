/** $RCSfile: imago_signal.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_signal.h,v 1.4 2003/03/21 13:12:03 gautran Exp $ 
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

#ifndef __IMAGO_SIGNAL_H_
#define __IMAGO_SIGNAL_H_


#ifdef _REENTRANT
void imago_signal_async_wait();
#define imago_signal_wait imago_signal_async_wait
#else
#define imago_signal_wait pause
#endif

typedef void (*imago_signal_t)( int, void * );

void imago_signal_init();
void imago_signal( int,  void (*)(int, void*), void * );
/** int  imago_signal_read_reset();  deprecated. */
/** void imago_signal_safe_handler( int );  deprecated. */
int  imago_signal_poll();
int imago_signal_kill( int );


#endif
