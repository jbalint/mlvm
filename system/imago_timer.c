/** $RCSfile: imago_timer.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_timer.c,v 1.4 2003/03/21 13:12:03 gautran Exp $ 
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
/************************************************/
/*           imago_timer.c                        */
/************************************************/

#include "system.h"
#include "opcode.h"
#include "engine.h"
#include "timer.h"


icb_queue internal_queue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
timer_id wakeup_timer = -1;
void timer_recall(timer_id, void *);
void timer_flush();

/************************************************/
/* A system thread responsible for handling     */
/* timer task in imago.                         */
/************************************************/

int imago_timer(){
    ICBPtr ip;
    struct timeval tv, tv1;
    struct timezone tz;
    int v = 0;

    icb_get(timer_queue, ip);
  
    if( ip == NULL || ip->status == IMAGO_INVALID ) {
      if( mlvm_stop ) timer_flush();
      return 0;
    }


    debug("pthread[%ld]: %s(ip=%p) \n", pthread_self(), __PRETTY_FUNCTION__, ip);
    /** If the system is shutting down, we terminate all imago. */
    if( mlvm_stop ) {
	ip->status = IMAGO_READY;
	icb_put_signal(engine_queue, ip);
	return 1;
    }

    // check imago status
  
    switch (ip->status) {
    case IMAGO_TIMER: 
      timerclear( &tv1 );
      gettimeofday( &tv, &tz );
      /** There are two cases, if dday has been initialized already by the builtin, 
	  do not do it all over again so the sleeping time is much more precise. */
      if( !timerisset(&ip->dday) ) {
	/** Grab the amount of time the imago desires to sleep. */
	/** First we dereference the stack pointer so we can get the integer value. */
	deref( v, ip->st );
	/** Then, follow the link to the value, either int or float. */
	if( is_int(v) ) {
	  /** Grab the integer value and store it as a double (without fractionnal value) */
	  tv1.tv_sec = int_value(v);
	} else if( is_float(v) ) {
	  double d = float_value( v );
	  tv1.tv_sec = (int)d;
	  /** fractionnal part in microsecond (10e6). */
	  tv1.tv_usec = (int)((d - (double)tv1.tv_sec) * 1000000.0);
	} else {
	  /** Neither int nor float means instanciation error. */
	  ip->status = IMAGO_ERROR;
	  ip->nl = E_ILLEGAL_ARGUMENT;
	  icb_put_signal( engine_queue, ip );
	  break;
	}
	/** Otherwise, compute the dday when this imago will have to be waken up... */
	timeradd( &tv, &tv1, &ip->dday );
      } else {
	timersub( &ip->dday, &tv, &tv1 );
      }
      /** If the value provided is either zero or negative, don't go through all that crap. */
      if( timercmp(&ip->dday, &tv, <) ) {
	timerclear( &ip->dday );
	ip->status = IMAGO_READY;
	icb_put_signal( engine_queue, ip );
	break;	
      }
      debug( "Imago %s scheduled for a sleep of %d sec and %d microsec.\n", ip->name, tv1.tv_sec, tv1.tv_usec );
      /** Put this ICB into the waiting list */
      icb_put_q( &internal_queue, ip );
      log_lock();
      ip->log->status = LOG_SLEEPING;
      log_unlock();
      /** Invoke the timer callback to recompute the earliest deadline. */
      timer_recall( wakeup_timer, ip );
      /** All set. */
      break;
    default: 
      /** Not a valid status. */
      icb_put_signal( engine_queue, ip );
      break;
    }
    return 1;
}

void timer_recall(timer_id id, void *arg) {
  /** Blindly scan the internal_queue to check which ICB must be waken up next. */
  struct timeval tv, tv1, tv2;
  struct timezone tz;
  ICBPtr wip;
  
  /** Do not forget to lock the queue. */
  que_lock(&internal_queue);
 restart:
  gettimeofday( &tv, &tz );
  for( wip = icb_head(&internal_queue); wip; wip = icb_next(wip) ) {
    icb_lock(wip);
    if( timercmp(&wip->dday, &tv, <) ) {
      /** Expired. */
      icb_remove( &internal_queue, wip );
      /** The imago may have been killed, so do not revive him now. */
      if( wip->status == IMAGO_TIMER ) 
	wip->status = IMAGO_READY;
      timerclear( &wip->dday );
      log_lock();
      wip->log->status = LOG_ALIVE;
      log_unlock();
      icb_put_signal( engine_queue, wip );
      /** restart from scratch since the queue has changed state. */
      goto restart;
    }
    icb_unlock(wip);
  }

  /** If the queue is empty, do not do anything... */
  if( que_empty(&internal_queue) ) {
    /** Deallocate the timer is it has been allocated before. */
    if( wakeup_timer >= 0 ) {
      timer_free( wakeup_timer );
      wakeup_timer = -1;
    }
    que_unlock(&internal_queue);
    return;
  }
  /** we got more in the list, so find the earliest dday. */
  /** Use the very first ICB has lowest mark. */
  wip = icb_head(&internal_queue);
  tv1.tv_sec = wip->dday.tv_sec;
  tv1.tv_usec = wip->dday.tv_usec;
  /** Then, scan the remaining of the list until we find the earliest deadline. */
  for( wip = icb_next(wip); wip; wip = icb_next(wip) ) {
    if( timercmp(&wip->dday, &tv1, <) ) {
      /** before... */
      tv1.tv_sec = wip->dday.tv_sec;
      tv1.tv_usec = wip->dday.tv_usec;
    }
  }
  /** We are all good and we got the earliest deadline. Set the timer. */
  if( wakeup_timer < 0 && (wakeup_timer = timer_new()) < 0 ) {
    warning( "Unable to allocate a new timer: Imago may sleep forever...\n" );
  } else {    
    /** Get the time of day. */
    gettimeofday( &tv, &tz );
    /** Have we passed the deadline... */
    if( timercmp(&tv1, &tv, <) ) { 
      tv2.tv_sec = 0;
      tv2.tv_usec = 1;
    } else {
      /** Get the delay. */
      timersub( &tv1, &tv, &tv2 );
    }
    timer_set( wakeup_timer, &tv2, timer_recall, NULL );
  }
  /** All good. */
  que_unlock(&internal_queue);
}


/** Enever the server terminates, it has to flush all imago remaining in the system.. */
void timer_flush() {
  ICBPtr ip;
  
  /** Flush them all... */
  que_lock(&internal_queue);
  while (que_not_empty(&internal_queue)){
    ip = icb_first(&internal_queue);
    icb_lock(ip);
    ip->status = IMAGO_READY;
    log_lock();
    ip->log->status = LOG_ALIVE;
    log_unlock();
    icb_put_signal( engine_queue, ip );
    debug("TIMER FLUSHING %s (ip=%p).\n", ip->name, ip);
  }
  que_unlock(&internal_queue);
  
}
