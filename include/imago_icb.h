/** $RCSfile: imago_icb.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_icb.h,v 1.15 2003/03/21 13:12:03 gautran Exp $ 
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
/*	 	imago_icb.h                     */
/************************************************/

#ifndef __IMAGO_ICB_H_
#define __IMAGO_ICB_H_

#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <netinet/in.h>

/************************************************/
/*	 	 Imago types                    */
/************************************************/

typedef enum {
    STATIONARY,
    WORKER,
    MESSENGER,
    SYS_MSGER
} imago_type;

#define is_messenger(IP) (((IP)->type == MESSENGER) || ((IP)->type == SYS_MSGER)) 
#define is_worker(IP) ((IP)->type == WORKER) 
#define is_stationary(IP) ((IP)->type == STATIONARY) 
#define is_sysmsger(IP) ((IP)->type == SYS_MSGER)


/************************************************/
/*	 	 Imago status                   */
/************************************************/

typedef enum {
    IMAGO_INVALID,
    IMAGO_ERROR,
    IMAGO_READY,
    IMAGO_EXPANSION,
    IMAGO_CONTRACTION,
    IMAGO_COLLECTION,
    IMAGO_SYS_CREATION,
    IMAGO_BACKTRACK,
    IMAGO_TIMER,
    IMAGO_CREATION,
    IMAGO_CLONING,
    IMAGO_MOVE_IN,
    IMAGO_MOVE_OUT,
    IMAGO_DISPATCHING,
    IMAGO_WAITING,
    IMAGO_ATTACHING,
    IMAGO_DISPOSE,
    IMAGO_TERMINATE
} imago_status;

typedef struct imago_control_block ICB;
typedef struct imago_control_block* ICBPtr;

typedef struct icb_queue_type icb_queue;

typedef struct host_type host;

typedef struct system_log_type LOG;
typedef struct system_log_type* LOGPtr;

typedef struct log_queue_type log_queue;


/************************************************/
/*	 	 Imago control block            */
/************************************************/

# define MAX_DNS_NAME_SIZE 80

struct host_type {
    char name[MAX_DNS_NAME_SIZE];
    long host_id;
};

struct icb_queue_type{
    ICBPtr			head;
    ICBPtr			tail;
    pthread_mutex_t lock;
    pthread_cond_t  cond;	
};

struct imago_control_block{
    ICBPtr              next;        // queuing pointer

    int		        application; // application id
    char		name[80];    // symbolic name
    imago_type	        type;        // thread type (worker, system, messenger)
    imago_status 	status;
    int 		expansions;  // number of expansions conducted
    int		        collections; // number of GC completed
    int                 jumps;       // number of jumps
 
    int*		block_base;	// All-In-One block
    int* 		block_ceiling;  // block_ceiling - block_base = block_size

    char*        	name_base; // name_base == block_base
    char*		name_top;  // some free space in between name_top and proc_base
    int*		proc_base; // [proc_base -> code_base)
    int*		code_base; // [code_base -> stack_base)
    int* 		stack_base;

    // registers section
    int*		pp;	// program pointer
    int*		af;	// active frame pointer
    int*		cp;	// continuation program pointer
    int*		cf;	// continuation frame pointer
    int*		st;	// stack pointer (successive put pointer)
    int*		bb;	// backtrack frame pointer
    int*		b0;	// cut backtrack frame pointer
    int*		gl;	// generation line
    int**		tt;	// trail pointer
    int		        nl;     // temp

    LOGPtr       	log;    // Log entry pointer

    icb_queue	        msq;    // messenger queue

    host                queen;  // stationary host

    /** Citizenship of a messenger or clone (where it has been born)
	(host_id not used) */
    host               citizenship;

    // temporary var for tracing
    int call_seq;
    int start_num;
    int call_count;
    int mid_call;
    
    /** The standard input, output and err for this ICB. */
    int cin;
    int cout;
    int cerr;

  /** Used when imago are sleeping for a while. */
  struct timeval dday;

    /** ICB locking mechanism. 
	MUST BE LAST IN THE STRUCTURE */
    pthread_mutex_t lock;
    /** DO NOT ADD ANYTHING BEHIND THIS LINE... */
};


#endif
