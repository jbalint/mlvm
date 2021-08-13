/** $RCSfile: imago_creator.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_creator.c,v 1.14 2003/03/21 13:12:03 gautran Exp $ 
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
/*	 	  imago_creator.c               */
/************************************************/

#include "system.h"

int imago_loader(ICBPtr);
int imago_sys_loader(ICBPtr);
int imago_cloner(ICBPtr);
int imago_dispatcher(ICBPtr);
int imago_unmarshaller(ICBPtr);

/************************************************/
/* A system thread responsible for imago        */
/* creation, cloning, and dispatching. It will  */
/* allocate the initial memory block for an     */
/* imago, set up ICB, and insert an imago's ICB */
/* into ready queue.                            */
/************************************************/

int imago_creator() {
  int r = 0;
  ICBPtr ip;

  icb_get(creator_queue, ip);

  if( ip == NULL || ip->status == IMAGO_INVALID )
    return 0;

  debug("pthread[%ld]: %s(ip=%p) \n", pthread_self(), __PRETTY_FUNCTION__, ip);
  /** If the system is shutting down, we terminate all non messenger imago. */
  if( mlvm_stop && !is_messenger(ip) ) {
      ip->status = IMAGO_TERMINATE;
      icb_put_signal(out_queue, ip);
      return 1;
  }

  debug("CREATOR: get %s to create\n", ip->name);

  switch (ip->status){
    // ip->name is the stationary's file name
    caseop(IMAGO_CREATION):	 r = imago_loader(ip); 
    break;
    // ip->name is the worker's file name
    caseop(IMAGO_SYS_CREATION): r = imago_sys_loader(ip); 
    break;
    // ip gives the cloning imago
    caseop(IMAGO_CLONING):	r = imago_cloner(ip); 
    break;
    // ip->name is the messenger's name
    caseop(IMAGO_DISPATCHING): r = imago_dispatcher(ip); 
    break;
    // ip->block_base is the buffer to be unmarshalled
    //    caseop(IMAGO_MOVE_IN): r = imago_unmarshaller(ip); 
    //    break;               
   
  default:	warning("CREATOR: wrong imago type\n");
    r = E_ILLEGAL_STATUS;
  }

 
  if (r == E_SUCCEED){
    debug("imago_creator: imago %s has created\n", ip->name);

    ip->status = IMAGO_READY;
 
    /* awake engine */

    icb_put_signal(engine_queue, ip);
  }
  else if (r == E_MEMORY_OUT){
    debug("imago_creator: INSERT %s to waiting_mem_queue\n", ip->name);
    icb_put_q( waiting_mem_queue, ip );
  }
  else{	
    ip->status = IMAGO_ERROR;
    ip->nl = r;
    icb_put_signal(out_queue, ip);
  } 
  return 1;
}
