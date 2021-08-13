/** $RCSfile: state_machine.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: state_machine.c,v 1.5 2003/03/21 13:12:03 gautran Exp $ 
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
#include <stdlib.h>
#include <string.h>

#include "include.h"
#include "state_machine.h"

#ifdef _REENTRANT
/** In order to protect the states from illegal use... */
pthread_mutex_t state_machine_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static state_id_t(**state_machine_table)(void *) = NULL;
static int state_machine_state_max = 0;
static int state_machine_state_count = 0;


void state_machine_init() {
  /** Initialize a initial number of states. */
  if( (state_machine_table = (state_action_t*)malloc(STATE_MACHINE_INCR * sizeof(void *))) == NULL ) {
    critical( "Unable to initialize the Imago State Machine.\n" );
    exit(-1);
  }
  /** Set the upper bound limit. */
  state_machine_state_max = STATE_MACHINE_INCR;
  /** Reset the count, just in case... */
  state_machine_state_count = 0;
}
void state_machine_cleanup() {
  if( state_machine_table )
    free(state_machine_table);
  state_machine_table = NULL;
}

/**
 * Registering a new state in the state machine.
 * 
 *  The return value is the state_id_t where the new state has been stored to
 *  if success or -1 if failure.
 */
state_id_t state_machine_register( state_action_t fcntl ) {
#ifdef _REENTRANT
  /** Just in case this may be called AFTER threads are started... */
  pthread_mutex_lock( &state_machine_mutex );
#endif
  /** First, make sure we do have enough room for the new state. */
  if( state_machine_state_count <= state_machine_state_max ) {
    /** Allocate more room. */
    void *ptr = malloc( (state_machine_state_max + STATE_MACHINE_INCR) * sizeof(void *) );
    if( ptr == NULL ) {
      warning( "State Machine is running low in memory.\n" );
#ifdef _REENTRANT
      /** Just in case this may be called AFTER threads are started... */
      pthread_mutex_unlock( &state_machine_mutex );
#endif
      return -1;
    }
    /** Move the data. */
    memcpy( ptr, state_machine_table, state_machine_state_max * sizeof(void *) );
    /** Update the pointer. */
    if( state_machine_table ) 
      free( state_machine_table );
    state_machine_table = ptr;
    /** Update the max. */
    state_machine_state_max = state_machine_state_max + STATE_MACHINE_INCR;
  }
  /** We got enough room. */
  state_machine_table[state_machine_state_count] = fcntl;
  /** Increment the count. */
  state_machine_state_count++;
#ifdef _REENTRANT
  /** Just in case this may be called AFTER threads are started... */
  pthread_mutex_unlock( &state_machine_mutex );
#endif
  /** return the location of the new state. */
  return (state_machine_state_count-1);
}

/**
 *  Deregistering a state from the state machine. 
 *  The deregistered state will be replaced by a null pointer.
 */
void state_machine_deregister( state_id_t id ) {
#ifdef _REENTRANT
  /** Just in case this may be called AFTER threads are started... */
  pthread_mutex_lock( &state_machine_mutex );
#endif
  /** Clear up the entry within the state machine table... */
  if( id >= 0 && id < state_machine_state_count ) 
    state_machine_table[id] = NULL;
#ifdef _REENTRANT
  /** Just in case this may be called AFTER threads are started... */
  pthread_mutex_unlock( &state_machine_mutex );
#endif
}


/** 
 * This is the state machine main function. 
 */
state_id_t state_machine( struct state_machine_str *state, void *data ) {
  state_action_t fcntl = NULL;
  state_id_t newstate;
  /** The state variable MUST be not null. */
  if( !state ) 
    return -1;
  /** Debug print the states. */
  debug( "State Machine: [%d]->[%d]\n", state->oldstate, state->state );

  /** Make sure the state is within the range. */
  if( state->state < 0 || state->state >= state_machine_state_count ) {
    warning( "State Machine: the state id is out of range (%d).\n", state->state );
    return -1;
  }
#ifdef _REENTRANT
  /** Just in case the register and deregister functions are called AFTER threads are started... */
  pthread_mutex_lock( &state_machine_mutex );
#endif
  /** See if there is any action associated with this state. */
  fcntl = state_machine_table[state->state];
#ifdef _REENTRANT
  /** Just in case the register and deregister functions are called AFTER threads are started... */
  pthread_mutex_unlock( &state_machine_mutex );
#endif
  /** Default, the new state is the current one. */
  newstate = state->state;
  /** Execute the state. */
  newstate = fcntl( data );
  /** Update the state. */
  if( newstate > 0 ) {
    state->oldstate = state->state;
    state->state = newstate;
  }
  return newstate;
}

