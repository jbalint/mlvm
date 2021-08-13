/** $RCSfile: timer.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: timer.c,v 1.10 2003/03/21 13:12:03 gautran Exp $ 
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
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "include.h"
#include "timer.h"
#include "imago_signal.h"


struct timer_str {
  timer_st status;
  struct timeval dday;
  void (*callback_cb)( timer_id, void *);
  void *arg_cb;
};

struct timer_str timer_tlb[MAX_TIMER];
#ifdef _REENTRANT
pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

void SIGALRM_handler( int, void * );

void timer_init() {
  /** Initialize the timer table. */
  memset( timer_tlb, 0, sizeof(timer_tlb) );
  /** Set outself to receive the SIGALRM. */
  imago_signal( SIGALRM, SIGALRM_handler, NULL );
}

void timer_activate() {
  timer_id id;
  struct timeval tv1;
  struct itimerval it;
  debug( "pthread [%ld]: %s()\n", pthread_self(), __PRETTY_FUNCTION__ ); 
  /** Reset the clock. */
  timerclear( &tv1 );
  memset( &it, 0, sizeof(it) );

#ifdef _REENTRANT
  pthread_mutex_lock( &timer_lock );
#endif
  /** Recompute the earliest deadline, then set the alarm so we get the SIGALARM when the time comes. */
  for( id = 0; id < MAX_TIMER; id++ ) {
    if( timer_tlb[id].status != TIMER_ACTIVE )
      continue;
    if( !timerisset(&tv1) || timercmp(&timer_tlb[id].dday, &tv1, <) ) {
	/** Before... */
      tv1.tv_sec = timer_tlb[id].dday.tv_sec;
      tv1.tv_usec = timer_tlb[id].dday.tv_usec;
    } else {
      /** Way after... */
    }
  }
  
  if( !timerisset(&tv1) ) {
    /** That's ok, don't need to reset the timer... 
	Besides, it is easier to receive the signal rather than
	reset the timer. */
    /** Actually, if you do not reset it, it keeps on going'n going ! */
    debug( "Resetting the itimer (not used).\n" );
  } else {
    struct timezone tz;
    struct timeval tv;
    /** Get the time of day. */
    gettimeofday( &tv, &tz );
    /** Make sure we did not pass the deadline. */
    if( timercmp(&tv, &tv1, <) ) {
	/** Before... */
      timersub( &tv1, &tv, &it.it_value );
    } else {
      /** Passed it... */
      it.it_value.tv_sec = 0;
      it.it_value.tv_usec = 1;
    }
    /** If we passed the deadline, the timer will fire right away... */
    debug( "Resetting itimer Alarm to fire in %d second(s) %d microsecond(s).\n", 
	   it.it_value.tv_sec, it.it_value.tv_usec );
  }
  setitimer( ITIMER_REAL, &it, NULL );
#ifdef _REENTRANT
  pthread_mutex_unlock( &timer_lock );
#endif
}

void SIGALRM_handler( int sig, void *sig_arg ) {
  timer_id id;
  struct timeval tv;
  struct timezone tz;
  void (*callback)( timer_id, void *) = NULL;
  void *arg_cb = NULL;
  debug( "%s(%d, %p)\n", __PRETTY_FUNCTION__, sig, sig_arg ); 
  /** Scan through the list of all timers so we can fire all of those who have expired. */
#ifdef _REENTRANT
  pthread_mutex_lock( &timer_lock );
#endif
  gettimeofday( &tv, &tz );
  for( id = 0; id < MAX_TIMER; id++ ) {
    if( timer_tlb[id].status == TIMER_ACTIVE ) {
      if( timercmp(&timer_tlb[id].dday, &tv, <) ) {
	/** Expired. */
	timer_tlb[id].status = TIMER_FIRED;
	callback = timer_tlb[id].callback_cb;
	arg_cb = timer_tlb[id].arg_cb;
	/** If a callback method is set, use it. */
	if( callback ) {
	/** Release the lock while processing the handler.
	    Some handler my want to set other timer or even free this one. */
#ifdef _REENTRANT
	  pthread_mutex_unlock( &timer_lock );
#endif
	  callback( id+1, arg_cb );
#ifdef _REENTRANT
	  pthread_mutex_lock( &timer_lock );
#endif
	}
      }
    }
  }
#ifdef _REENTRANT
  pthread_mutex_unlock( &timer_lock );
#endif
  /** Finally, recompute the earliest deadline. */
  timer_activate();
}


timer_id timer_new() {
  timer_id id;
#ifdef _REENTRANT
  pthread_mutex_lock( &timer_lock );
#endif
  for( id = 0; id < MAX_TIMER; id++ ) {
    if( timer_tlb[id].status == TIMER_FREE ) {
      timer_tlb[id].status = TIMER_STOPPED;
      break;
    }
  }
#ifdef _REENTRANT
  pthread_mutex_unlock( &timer_lock );
#endif
  return (id < MAX_TIMER)? id+1: TIMER_INVALID;
}

void timer_free( timer_id id ) {
  /** Make sure the ID is valid. */
  id--;
  if( id < 0 || id >= MAX_TIMER ) 
    return;
#ifdef _REENTRANT
  pthread_mutex_lock( &timer_lock );
#endif
  /** Clear the structure. */
  memset( &timer_tlb[id], 0, sizeof( struct timer_str ) );
#ifdef _REENTRANT
  pthread_mutex_unlock( &timer_lock );
#endif
  /** If this was the earliest deadline, recompute it. */
  //  if( earliest == id ) 
  timer_activate();
}

int timer_set( timer_id id, const struct timeval *tv, void (*cb)(timer_id, void *), void *arg_cb  ) {
  struct timezone ntz;
  struct timeval ntv;
  id--;
  /** Make sure the ID is valid and the timeval structure is present. */
  if( id < 0 || id >= MAX_TIMER || tv == NULL ) 
    return TIMER_INVALID;
#ifdef _REENTRANT
  pthread_mutex_lock( &timer_lock );
#endif
  /** Make sure the timer as been allocated. */
  if( timer_tlb[id].status == TIMER_FREE ) {
#ifdef _REENTRANT
    pthread_mutex_unlock( &timer_lock );
#endif
    return -1;    
  }
  /** Get the time of day. */
  gettimeofday( &ntv, &ntz );
  /** Copy the timeval structure, call back routine and arg (if any). */
  timeradd( &ntv, tv, &timer_tlb[id].dday );
  if( cb ) timer_tlb[id].callback_cb = cb;
  if( cb ) timer_tlb[id].arg_cb = arg_cb;
  /** Change this timer status to active. */
  timer_tlb[id].status = TIMER_ACTIVE;
#ifdef _REENTRANT
  pthread_mutex_unlock( &timer_lock );
#endif
  /** Recompute the earliest deadline. */
  timer_activate();
  return 0;
}

timer_st timer_status( timer_id id ) {
  timer_st st;
  id--;
  /** Make sure the ID is valid and the timeval structure is present. */
  if( id < 0 || id >= MAX_TIMER ) 
    return TIMER_INVALID;
#ifdef _REENTRANT
  pthread_mutex_lock( &timer_lock );
#endif
  st = timer_tlb[id].status;
#ifdef _REENTRANT
  pthread_mutex_unlock( &timer_lock );
#endif
  return st;
}

void timer_stop( timer_id id ) {
  id--;
  /** Make sure the ID is valid and the timeval structure is present. */
  if( id < 0 || id >= MAX_TIMER ) 
    return;
#ifdef _REENTRANT
  pthread_mutex_lock( &timer_lock );
#endif  
  timer_tlb[id].status = TIMER_STOPPED;
#ifdef _REENTRANT
  pthread_mutex_unlock( &timer_lock );
#endif
  /** If this was the earliest deadline, recompute it. */
  //  if( earliest == id )
  timer_activate();
}
