/** $RCSfile: state_machine.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: state_machine.h,v 1.3 2003/03/21 13:12:03 gautran Exp $ 
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

#ifndef __STATE_MACHINE_H_
#define __STATE_MACHINE_H_

#define STATE_MACHINE_ID_INVALID -1

typedef int state_id_t;
typedef state_id_t(*state_action_t)(void *);

struct state_machine_str {
  state_id_t oldstate;
  state_id_t state;
};


void state_machine_init();
void state_machine_cleanup();

state_id_t state_machine_register( state_action_t );
void state_machine_deregister( state_id_t );
state_id_t state_machine( struct state_machine_str *, void * );


#endif
