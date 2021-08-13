/** $RCSfile: timer.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: timer.h,v 1.2 2003/03/21 13:12:03 gautran Exp $ 
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
#include <sys/time.h>

#ifndef __TIMER_H_
#define __TIMER_H_

#define MAX_TIMER 255

#define TIMER_INVALID -1

#define MIN * 60
#define MINS MIN
#define SEC * 1

typedef enum {
  TIMER_FREE = 0,
  TIMER_ACTIVE,
  TIMER_FIRED,
  TIMER_STOPPED,
} timer_st;

typedef int timer_id;

void timer_init();

timer_id timer_new();
void timer_free( timer_id );

int timer_set( timer_id, const struct timeval *, void (*)(timer_id, void *), void * );
timer_st timer_status( timer_id );
void timer_stop( timer_id );


#endif
