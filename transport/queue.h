/** $RCSfile: queue.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: queue.h,v 1.2 2003/03/21 13:12:03 gautran Exp $ 
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


#ifndef __QUEUE_H_
#define __QUEUE_H_

struct link_str {
  struct link_str *next;
};

struct link_head_str {
  struct link_str *first;
  struct link_str *last;
#ifdef _REENTRANT
  pthread_mutex_t lock;
#endif
};


void queue_init( struct link_head_str * );
void enqueue( struct link_head_str *, struct link_str * );
void push( struct link_head_str *, struct link_str * );
struct link_str *dequeue( struct link_head_str * );
struct link_str *pop( struct link_head_str * );
struct link_str *first( struct link_head_str * );
struct link_str *last( struct link_head_str * );
struct link_str *next( struct link_head_str *, struct link_str * );
int size( struct link_head_str * );
#endif
