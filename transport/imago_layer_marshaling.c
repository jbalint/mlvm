/** $RCSfile: imago_layer_marshaling.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_marshaling.c,v 1.16 2003/03/24 14:07:48 gautran Exp $ 
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
/** ############################################################################
 *     Marshaling PDU Header
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                     Application ID                            |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |  Imago Type   |                                               |  | 
 * +---------------+   Imago Symbolic Name                       +-/  |Imago 
 * |                                                             |0|  |Identity 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                                                               |  | 
 * /                  Queen Imago Server Name                    +-/  | 
 * |                                                             |0|  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                     Queen's host id                           |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                                                               |  | 
 * /              Citizenship (clone and messengers only)        +-/  | 
 * |                                                             |0|  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |               Citizenship server host id                      |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                       Jumps count                             |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                      trail pointer                            |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |  
 * |                     generation line                           |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                cut backtrack frame pointer                    |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                   backtrack frame pointer                     |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |Imago 
 * |            stack pointer (successive put pointer)             |  |Registers 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                 continuation frame pointer                    |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                continuation program pointer                   |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                    active frame pointer                       |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                      program pointer                          |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                         block base                            |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                         code base                             |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                         name base                             |  |All 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |In 
 * |                         name top                              |  |One 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |Block 
 * |                         stack base                            |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |                         block ceiling                         |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                                                               |  | 
 * /                            data                               /  |Imago 
 * |                                                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 *  
 *  
 * ########################################################################## */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include.h"
#include "opcode.h"
#include "imago_icb.h"
#include "imago_pdu.h"
#include "state_machine.h"
#include "imago_layer_ADArendezvous.h"
#include "imago_layer_marshaling.h"


ICBPtr icb_new();
void icb_free(ICBPtr);

/** State machine ids. */
state_id_t imago_layer_marshaling_up_stateid = -1;
state_id_t imago_layer_marshaling_down_stateid = -1;
/** */

state_id_t imago_layer_marshaling_up( struct pdu_str *);
state_id_t imago_layer_marshaling_down( struct pdu_str *);

void block_adjust( register struct imago_control_block *, register int );


/** Initialization routine called once at the very first begining. */
void imago_layer_marshaling_init() {
  /** Register the states in the state machine. */
  imago_layer_marshaling_up_stateid = state_machine_register( (state_action_t)imago_layer_marshaling_up );
  imago_layer_marshaling_down_stateid = state_machine_register( (state_action_t)imago_layer_marshaling_down );
  /** */

}
/** */
int imago_layer_marshaling_getopt( int opt , void *arg ) {
  return -1;
}
int imago_layer_marshaling_setopt( int opt, void *arg ) {
  return -1;
}


void imago_dump( struct imago_control_block *this_icb ) {
  //  register unsigned char *ax, *bx;

  /** Debug print all the important information. */
  debug( "All-In-One block\n");
  debug( "\tblock_ceiling: %p\n", this_icb->block_ceiling );
  debug( "\tstack_base:    %p\n", this_icb->stack_base );
  debug( "\tname_top:      %p\n", this_icb->name_top );
  debug( "\tname_base:     %p\n", this_icb->name_base );
  debug( "\tcode_base:     %p\n", this_icb->code_base );
  debug( "\tproc_base:     %p\n", this_icb->proc_base );
  debug( "\tblock_base:    %p\n", this_icb->block_base );

  debug( "Registers\n");
  debug( "\tpp:            %p\n", this_icb->pp );
  debug( "\taf:            %p\n", this_icb->af );
  debug( "\tcp:            %p\n", this_icb->cp );
  debug( "\tcf:            %p\n", this_icb->cf );
  debug( "\tst:            %p\n", this_icb->st );
  debug( "\tbb:            %p\n", this_icb->bb );
  debug( "\tb0:            %p\n", this_icb->b0 );
  debug( "\tgl:            %p\n", this_icb->gl );
  debug( "\ttt:            %p\n", this_icb->tt );
  /** */
}

/* Block layout */
/********************************************************

      before
     -------------- <- block_ceiling
     | trail      |
     |------------|
     |            |
     |-----^------| <- stack_top
     |            |
     | stack      |
     |            |
     |------------| <- stack_base
     |            |
     |-----^------| <- name_top
     | name_table |
     |------------| <- name_base
     | code       |
     |------------| <- code_base
     | proc_table |
     -------------- <- block_base


     after
     -------------- <- block_ceiling
     |            |
     | free       |
     |            |
  -> |------------| 
 |   | trail      |
 |   |------------| <- stack_top
 |   |            |
 |   | stack      |
 |   |            |
P|   |------------| <- stack_base
D|   |            |
U|   |-----^------| <- name_top
 |   | name_table |
 |   |------------| <- name_base
 |   | code       |
 |   |------------| <- code_base
 |   | proc_table |
  -> -------------- <- block_base


*********************************************************/


state_id_t imago_layer_marshaling_down( struct pdu_str *this_pdu ) {
  /** Just for ease of use. */
  struct imago_control_block *this_icb = this_pdu->icb_ptr;

  debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 

  /** Dump the ICB to the screen to verify the data. */
  imago_dump( this_icb );
  /** */

  /** Add the Imago memory block to be transmitted starting with the trail. */
  if( pdu_write_string(this_pdu, (char *)this_icb->tt, (char *)this_icb->block_ceiling - (char*)this_icb->tt) < 0 )
    goto error;
  
  /** Then the stack_base up to the stack top (omitting the free space and the trail already written). */
  if( pdu_write_string(this_pdu, (char *)this_icb->stack_base, (char *)this_icb->st - (char*)this_icb->stack_base) < 0 )
    goto error;

  /** Then the block_base up to the name top (omitting the free space, the stack and the trail already written). */
  if( pdu_write_string(this_pdu, (char *)this_icb->block_base, (char *)this_icb->name_top - (char*)this_icb->block_base) < 0 )
    goto error;

  /** */

  /** Now, put the different elements into the pdu header. */
  /** The first group is the base pointers. */
  /** block ceiling. */
  if( pdu_write_int( this_pdu, (int)this_icb->block_ceiling ) < 0 ) goto error; 
  /** stack base. */
  if( pdu_write_int( this_pdu, (int)this_icb->stack_base ) < 0 ) goto error;
  /** name top. */
  if( pdu_write_int( this_pdu, (int)this_icb->name_top ) < 0 ) goto error;
  /** name base. */
  if( pdu_write_int( this_pdu, (int)this_icb->name_base ) < 0 ) goto error;
  /** code base. */
  if( pdu_write_int( this_pdu, (int)this_icb->code_base ) < 0 ) goto error;
  /** block base. */
  if( pdu_write_int( this_pdu, (int)this_icb->block_base ) < 0 ) goto error;
  /** We do not worry about the trail because it is located between the stack top and the ceiling. */
  /** */

  /** The second group represent the imago registers. */
  /** program pointer. */
  if( pdu_write_int( this_pdu, (int)this_icb->pp ) < 0 ) goto error;
  /** active frame. */
  if( pdu_write_int( this_pdu, (int)this_icb->af ) < 0 ) goto error;
  /** continuation program pointer. */
  if( pdu_write_int( this_pdu, (int)this_icb->cp ) < 0 ) goto error;
  /** continuation frame pointer. */
  if( pdu_write_int( this_pdu, (int)this_icb->cf ) < 0 ) goto error;
  /** stack top. */
  if( pdu_write_int( this_pdu, (int)this_icb->st ) < 0 ) goto error;
  /** backtrack frame pointer. */
  if( pdu_write_int( this_pdu, (int)this_icb->bb ) < 0 ) goto error;
  /** cut backtrack frame pointer. */
  if( pdu_write_int( this_pdu, (int)this_icb->b0 ) < 0 ) goto error;
  /** generation line. */
  if( pdu_write_int( this_pdu, (int)this_icb->gl ) < 0 ) goto error;
  /** trail pointer. */
  if( pdu_write_int( this_pdu, (int)this_icb->tt ) < 0 ) goto error;
  /** */


  /** The last group is the Imago identity. */
  /** What is jumps count for this Imago. */
  if( pdu_write_int( this_pdu, this_icb->jumps ) < 0 ) goto error;
  /** What is the Citizeship host id of this imago. */
  if( pdu_write_int( this_pdu, this_icb->citizenship.host_id ) < 0 ) goto error;
  /** What is the Citizeship of this imago. */
  if( pdu_write_text( this_pdu, this_icb->citizenship.name ) < 0 ) goto error;
  /** What is the Queen's host_id. */
  if( pdu_write_int( this_pdu, this_icb->queen.host_id ) < 0 ) goto error;
  /** What is the Queen's name. */
  if( pdu_write_text( this_pdu, this_icb->queen.name ) < 0 ) goto error;
  /** Also the Imago name. */
  if( pdu_write_text( this_pdu, this_icb->name ) < 0 ) goto error;
  /** Then, what type of Imago this is. */
  if( pdu_write_byte( this_pdu, this_icb->type & 0xff ) < 0 ) goto error;
  /** And last (or actually first), the application ID. */
  if( pdu_write_int( this_pdu, this_icb->application ) < 0 ) goto error;
  /** */

  /** We marshaled the ICB, we can release it now. */
  icb_free( this_icb );
  /** And clear some pointers. */
  this_pdu->icb_ptr = NULL;

  /** And forward all this to the ADA rendez-vous layer. */
  return imago_layer_ADArendezvous_down_stateid;

 error:
  /** Something went wrong. Abort and forward back the ICB to where it belongs. */
  this_pdu->flags |= PDU_ERROR;
  this_pdu->errno = EPDU_MFRAME;
  this_pdu->e_layer = imago_layer_marshaling_down_stateid;
  /** And return an invalid state. */
  return imago_layer_marshaling_up_stateid;
}

/** Un-marshal the incoming ICB. */
state_id_t imago_layer_marshaling_up( struct pdu_str *this_pdu ) {
  extern state_id_t imago_protocol_up_stateid;
  struct imago_control_block *this_icb = this_pdu->icb_ptr;
  char   *new_base;       // new block base pointer
  char   *old_base;       // old block base pointer
  int    distance;
  char   byte;
  debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
 
  /** Always make sure the old ICB has been freed. */
  if( !this_pdu->icb_ptr ) {
    /** First, try to get an Imago ICB for this new PDU. */
    if( (this_pdu->icb_ptr = icb_new()) == NULL ) {
      /** We were unable to allocate a new ICB. */
      warning( "Unable to allocate a new ICB.");
      /** 
       * TO DO: Deal with this case. 
       */
      /** Then, clean up the PDU. */
      pdu_free( this_pdu );
      /** And return an invalid state. */
      return STATE_MACHINE_ID_INVALID;
    }
    /** We got a fresh new ICB. */
    /** Just for ease of use. */
    this_icb = this_pdu->icb_ptr;
    /** Read the information from the PDU. */

    /** The first group is the Imago identity. */
    /** And last (or actually first), the application ID. */
    pdu_read_int( this_pdu, &this_icb->application );
    /** Then, what type of Imago this is. */
    pdu_read_byte( this_pdu, &byte );
    this_icb->type = byte;
    /** Also the Imago name. */
    pdu_read_text( this_pdu, this_icb->name, sizeof(this_icb->name) );
    /** What is the Queen's name. */
    pdu_read_text( this_pdu, this_icb->queen.name, sizeof(this_icb->queen.name) );
    /** What is the Queen's host_id. */
    pdu_read_int( this_pdu, (int*)&this_icb->queen.host_id );
    /** What is the citizenship of this imago. */
    pdu_read_text( this_pdu, this_icb->citizenship.name, sizeof(this_icb->citizenship.name) );
    /** What is the citizenship host id of this imago. */
    pdu_read_int( this_pdu, (int*)&this_icb->citizenship.host_id );
    /** What is jumps count for this Imago. */
    pdu_read_int( this_pdu, &this_icb->jumps );
    /** */


    /** The second group represent the imago registers. */
    /** trail top. */
    pdu_read_int( this_pdu, (int*)&this_icb->tt );
    /** generation line. */
    pdu_read_int( this_pdu, (int*)&this_icb->gl );
    /** cut backtrack frame pointer. */
    pdu_read_int( this_pdu, (int*)&this_icb->b0 );
    /** backtrack frame pointer. */
    pdu_read_int( this_pdu, (int*)&this_icb->bb  );
    /** stack top. */
    pdu_read_int( this_pdu, (int*)&this_icb->st );
    /** continuation frame pointer. */
    pdu_read_int( this_pdu, (int*)&this_icb->cf );
    /** continuation program pointer. */
    pdu_read_int( this_pdu, (int*)&this_icb->cp );
    /** active frame. */
    pdu_read_int( this_pdu, (int*)&this_icb->af );
    /** program pointer. */
    pdu_read_int( this_pdu, (int*)&this_icb->pp );
    /** */  
 
    /** The third group is the base pointers. */
    /** block base. */
    pdu_read_int( this_pdu, (int*)&this_icb->block_base );
    /** code base. */
    pdu_read_int( this_pdu, (int*)&this_icb->code_base );
    /** name base. */
    pdu_read_int( this_pdu, (int*)&this_icb->name_base );
    /** name top. */
    pdu_read_int( this_pdu, (int*)&this_icb->name_top );
    /** stack base. */
    pdu_read_int( this_pdu, (int*)&this_icb->stack_base );
    /** block ceiling. */
    pdu_read_int( this_pdu, (int*)&this_icb->block_ceiling ); 
    /** */

    /** The proc_base variable is actually the block_base. */
    old_base = this_icb->proc_base = this_icb->block_base;

    /** Just in case the PDU is not valid. */
    if( ((char*)this_icb->block_ceiling - (char*)this_icb->block_base) <= 0 ) {
	warning( "%s(%p): Invalid PDU.\n", __PRETTY_FUNCTION__, this_pdu );

	/** Clear up the pointers in the ICB block. */
	this_icb->block_base = NULL;

	icb_free( this_icb );
	pdu_free( this_pdu );
	/** And return an invalid state. */
	return STATE_MACHINE_ID_INVALID;
    }
  
    /** Now allocate enough memory to hold the entire "All-In-One" block. */
    if( (new_base = (char *)malloc((char*)this_icb->block_ceiling - (char*)this_icb->block_base)) == NULL ) {
      /** We ran out of memory. */
      warning( "System is low in memory (requesting %d bytes).\n", 
		   (char*)this_icb->block_ceiling - (char*)this_icb->block_base);
      /**
       * TO DO: Handle this case. 
       */
      /** Clear up the pointers in the ICB block. */
      this_icb->block_base = NULL;

      icb_free( this_icb );
      pdu_free( this_pdu );
      /** And return an invalid state. */
      return STATE_MACHINE_ID_INVALID;
    }
    /** Now that we have the memory, adjust all pointers so it is done. */
    distance = new_base - old_base;         // get distance of two blocks in bytes
    debug( "New base = 0x%p, Old base = 0x%p, distance = %d\n", new_base, old_base, distance );


    /** Then, adjust the bases. */
    /** block base. */
    this_icb->block_base    += distance;
    /** proc base. */
    this_icb->proc_base     += distance;
    /** code base. */
    this_icb->code_base     += distance;
    /** name base. */
    this_icb->name_base     += distance;
    /** name top. */
    this_icb->name_top      += distance;
    /** stack base. */
    this_icb->stack_base    += distance;
    /** block ceiling. */
    this_icb->block_ceiling += distance;
    /** */

    /** And all the registers. */    
    /** trail top. */
    this_icb->tt += distance;
    /** generation line. */
    this_icb->gl += distance;
    /** cut backtrack frame pointer. */
    this_icb->b0 += distance;
    /** backtrack frame pointer. */
    this_icb->bb += distance;
    /** stack top. */
    this_icb->st += distance;
    /** continuation frame pointer. */
    this_icb->cf += distance;
    /** continuation program pointer. */
    this_icb->cp += distance;
    /** active frame. */
    this_icb->af += distance;
    /** program pointer. */
    this_icb->pp += distance;
    /** */  

    /** If we got the resource, load the imago memory block from the base to the name top. */
    pdu_read_string( this_pdu, (char *)this_icb->block_base, (char*)this_icb->name_top - (char*)this_icb->block_base );
    /** Load the stack. */
    pdu_read_string( this_pdu, (char *)this_icb->stack_base, (char*)this_icb->st - (char*)this_icb->stack_base );
    /** Then, load the trail to its normal place. */
    pdu_read_string( this_pdu, (char *)this_icb->tt, (char*)this_icb->block_ceiling - (char*)this_icb->tt  );
    /** */

    /** Last thing to do is to adjust all pointers in the trail and the other place as well. */
    block_adjust( this_icb, distance );
  }
  /** And off course it is to restore a correct Imago status. */
  this_icb->status = IMAGO_MOVE_IN;
  /**
   * Check for transmission errors.
   * If the Imago could not be send, it will come back this way,
   * So we need to check for errors. 
   */
  if( this_pdu->flags & PDU_ERROR ) {
    /** There was an error, the error code and layer number are indicated in the PDU. */
    this_icb->status = IMAGO_ERROR;
    warning( "Unable to send ICB.\nError Code: %d\nLocation: %d\n", 
		 this_pdu->errno, this_pdu->e_layer );

    /**
     * TO DO: return more information regarding the error type...
     */
  }
  
  /** Dump the ICB to the screen to verify the data. */
  imago_dump( this_icb );  
  /** */
  return imago_protocol_up_stateid;
}



#include "system.h"

#define mtype() (opcode_tab[*tp2].type)

#define mdirect()       {tp2++;}

#define madjust()       {*tp2++ += distance;}

#define mhash()         {tp2++;                            \
                        ax = *tp2 + 1;                     \
                        tp2++;                             \
                        while (ax--){                      \
                                *tp2 += distance;       \
                                tp2 += 2;                      \
                        }                                  \
                        tp2--;                             \
                        }

void block_adjust( register struct imago_control_block *plt, register int distance /** In bytes ! */) {
  register int*   tp1;            // temp pointer
  register int*   tp2;            // temp pointer
  register int    ax;
  register int    bx;

  // 0) Convert the distance in integer 
  distance /= sizeof(int);

  /****************************************************************/
  /*                                                              */
  /*                     move segments:                           */
  /* 1) procedure entry table must be adjusted                    */
  /* 2) code hast been moved but code entries should be adjusted  */
  /* 3) symbolic name table has been moved, no adjustment         */
  /* 4) active stack must be scanned and pointers be adjusted     */
  /****************************************************************/
  
  
  // 1) procedure table: adjust entries
  
  tp1 = plt->proc_base + 1;               // hit the entry slot
  tp2 = plt->code_base;                   // get the proc_table stop point
  while (tp1 < tp2){
    *tp1 += distance;
    tp1 += 2;                                       // advance to the next entry
  }
  
  // 2) code: only adjust code entries
  
  tp1 = (int*) plt->name_base;    // get code stop point
  
  while (tp2 < tp1){                              // tp2 starts from code_base
    adjust_code(mtype(),
		mdirect(),
		madjust(),
		mhash());
  }
  
  // 3) symbolic table: nothing to do

  // 4) Linear scan the active stack
  // a) bypass constants, a special case is that when
  // a FVL_TAG is met, we have to bypass three cells (a floating value)
  // b) adjust all tag_on_pointer terms  which include var, str,
  // list, and control pointers such as cp, cf, bb, etc.,
  // in control frames
  
  tp1 = plt->stack_base;   // tp1 points to stack_base
  tp2 = plt->st;                  // get new active stack top
  
  while (tp1 <= tp2){
    ax = *tp1;
    bx = tag(ax);
    if (cst_tag(bx)){
      if (is_fvl(ax)){ // bypass floating value
	tp1 += 3;
      }
      else{
	tp1++;
      }
    }
    else {
      ax = addr(ax) + distance;
      *tp1++ = ax | bx;
    }
  }

  // now we should adjust the trail
  ax = (plt->block_ceiling - (int *)plt->tt); // get active trail size
  tp1 = (int *)(plt->tt-1);
  while(ax--){
    bx = *(++tp1);
    if (is_var(bx)) {
      *tp1 = (int)((int*) bx + distance);
    }
    else {  // must be a generation line
      bx = (int) addr(bx);
      *tp1 = make_str((int*) bx + distance);
    }
  }
}
