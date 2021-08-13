/** $RCSfile: queue.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: queue.c,v 1.5 2003/03/25 23:31:14 gautran Exp $ 
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
#include <stdio.h>
#include <string.h>
#ifdef _REENTRANT
# include <pthread.h>
#endif

#include "queue.h"


struct link_str *first( struct link_head_str *_h ) {
  struct link_str *_n;
#ifdef _REENTRANT
  pthread_mutex_lock( &_h->lock );
  {
#endif
    /** Retreive the first node in the list. */
    _n = _h->first;
#ifdef _REENTRANT
  }
  pthread_mutex_unlock( &_h->lock );
#endif
  return _n;
}

struct link_str *last( struct link_head_str *_h ) {
  struct link_str *_n;
#ifdef _REENTRANT
  pthread_mutex_lock( &_h->lock );
  {
#endif
    /** Retreive the last node in the list. */
    _n = _h->last;
#ifdef _REENTRANT
  }
  pthread_mutex_unlock( &_h->lock );
#endif
  return _n;
}

struct link_str *next( struct link_head_str *_h, struct link_str *_p ) {
  struct link_str *_n;
#ifdef _REENTRANT
  pthread_mutex_lock( &_h->lock );
  {
#endif
    /** Retreive the next node in the list. */
    _n = (_p == _h->last)? NULL: _p->next;
#ifdef _REENTRANT
  }
  pthread_mutex_unlock( &_h->lock );
#endif
  return _n;
}
int size( struct link_head_str *_h ) {
  register struct link_str *_n;
  register int s = 0;
#ifdef _REENTRANT
  pthread_mutex_lock( &_h->lock );
  {
#endif
    /** Retreive the number of node in the list. */
    for( _n = _h->first; _n; _n = _n->next ) {
      s++;
    }
#ifdef _REENTRANT
  }
  pthread_mutex_unlock( &_h->lock );
#endif
  return s;
}

void insert_last( struct link_head_str *_h, struct link_str *_n ) {
#ifdef _REENTRANT
  pthread_mutex_lock( &_h->lock ); 
  {
#endif
    /** Append at the end of the queue. */
    if( _h->last == NULL ) {
      _h->last = _h->first = _n;
      _n->next = NULL;
    } else {
      _h->last->next = _n;
      _n->next = NULL;
      _h->last = _n;
    }
#ifdef _REENTRANT
  } 
  pthread_mutex_unlock( &_h->lock );
#endif
}
void insert_first( struct link_head_str *_h, struct link_str *_n ) {
#ifdef _REENTRANT
  pthread_mutex_lock( &_h->lock );
  {
#endif
    /** Append at the end of the queue. */
    if( _h->first == NULL ) {
      _h->last = _h->first = _n;
      _n->next = NULL;
    } else {
      _n->next = _h->first;
      _h->first = _n;
    }
#ifdef _REENTRANT
  }
  pthread_mutex_unlock( &_h->lock );
#endif
}
struct link_str *remove_first( struct link_head_str *_h ) {
  struct link_str *_n;
#ifdef _REENTRANT
  pthread_mutex_lock( &_h->lock );
  {
#endif
    /** Remove the first node in the list. */
    _n = _h->first;
    if( _n )
      _h->first = _n->next;

    if( !_h->first )
      _h->last = NULL;
#ifdef _REENTRANT
  }
  pthread_mutex_unlock( &_h->lock );
#endif
  return _n;
}

void queue_init( struct link_head_str *_h ) {
  memset( _h, 0, sizeof(struct link_head_str) );
}

void enqueue( struct link_head_str *_h, struct link_str *_n ) {
  insert_last( _h, _n );
}

void push( struct link_head_str *_h, struct link_str *_n ) {
  insert_first( _h, _n );
}

struct link_str *dequeue( struct link_head_str *_h ) {
  return remove_first( _h );
}

struct link_str *pop( struct link_head_str *_h ) {
  return remove_first( _h );
}
