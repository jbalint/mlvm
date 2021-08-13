/** $RCSfile: system.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: system.h,v 1.30 2003/03/25 13:39:25 gautran Exp $ 
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
	/*	 	system.h                            */
	/************************************************/

#ifndef SYSTEM_HH
#define SYSTEM_HH


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MEMWATCH
#include "memwatch.h"
#endif

#include "version.h"
#include "log.h"
#include "imago_icb.h"
#include "mlvm.h"
#include "transport.h"


#define caseop(X)	case X

	/************************************************/
	/*	 	 System Constants                   */
	/************************************************/

enum {  MAX_IMAGOES = 1024,
	MAX_LOGS = 1024,
	STACK_UNIT	= 1024,		// 1K
	INIT_MESSENGER_STACK	= 4,		// in  Kbytes
	INIT_WORKER_STACK	= 128,		// in  Kbytes
	INIT_STATIONARY_STACK	= 2048,		// in  Kbytes
	NAME_EXP_SIZE = 256, 	// byte
	BACKTRACK_LIMIT = 50000, // Maximum backtrack count prior to forced context switch
	};

 	/************************************************/
	/*	 	 System log facility                */
	/************************************************/

typedef enum {
  LOG_ALIVE,
  LOG_MOVED,
  LOG_DISPOSED,
  LOG_BLOCKED,
  LOG_SLEEPING
} log_status;



struct system_log_type {
 	int			application; // application id
	char			name[MAX_DNS_NAME_SIZE];    // symbolic name
	log_status		status;		// current status
	host			server;		// queen if alive or disposed, or
                                    // location if it moved
        int                     jumps;          // latest known number of jumps
	time_t			time;		// timestamp
	ICBPtr			ip;			// NULL if not alive
	LOGPtr			next;		// chaining pointer
};
struct log_queue_type {
	LOGPtr			head;
	LOGPtr			tail;
	pthread_mutex_t lock;
};
	/************************************************/
	/*	 	 log queue functions                */
	/************************************************/

void   log_init();                     /* initialize log pool */
void   log_cleanup();
LOGPtr log_new();
void   log_free(LOGPtr);              /* free a log */
void   log_append(LOGPtr); 		/* append to the end of live_logs */
LOGPtr log_find(long, int, char*);	/* find the imago from live_logs */
void   log_blocked( LOGPtr );
void   log_disposed( LOGPtr );
LOGPtr log_alive( LOGPtr, ICBPtr );
void   log_moved( LOGPtr, int, const char * );
void   print_log( FILE * );

#define log_lock()      pthread_mutex_lock(&(live_logs->lock))
#define log_unlock()    pthread_mutex_unlock(&(live_logs->lock))

#define que_lock(Q)         pthread_mutex_lock(&((Q)->lock))
#define que_unlock(Q)       pthread_mutex_unlock(&((Q)->lock))
#define que_append(Q,IP)    icb_append((Q),(IP))

 	/************************************************/
	/*	 	 System threads                     */
	/************************************************/
/** Number of threads. Must be greated or equal to 5 for better performances. */
#define MAX_SYSTEM_THREADS 5

int  imago_in();
void imago_in_cb( const char *, ICBPtr );
int  imago_out();
int  imago_timer();
int  imago_creator();
int  imago_memory();
int  imago_scheduler();
int  get_unique_appid();

extern int system_count;
extern pthread_mutex_t system_lock;
extern pthread_cond_t  system_cond;

 	/************************************************/
	/*	 	 System queues                      */
	/************************************************/

extern log_queue*		free_logs;
extern log_queue*		live_logs;
extern icb_queue*		free_queue;
extern icb_queue*		creator_queue;
extern icb_queue*		memory_queue;
extern icb_queue*		engine_queue;
extern icb_queue*		out_queue;
extern icb_queue*		in_queue;
extern icb_queue*		timer_queue;
extern icb_queue*		security_queue;
extern icb_queue*		waiting_mem_queue;
extern icb_queue*		waiting_msger_queue;


	/************************************************/
	/*	 	 ICB queue functions                */
	/************************************************/

void   icb_init();                       /* initialize icb pool */
void   icb_cleanup();                    /** Clean up the memory. */
ICBPtr icb_new();
void   icb_free(ICBPtr);
ICBPtr icb_first(icb_queue*);             /* get the first icb  */
#define icb_next(IP) (IP)->next           /** Get the next ICB. */
#define icb_head(IP) (IP)->head           /** Get the head ICB. */
void   icb_insert(icb_queue*, ICBPtr);    /* insert the icb to the front  */
void   icb_append(icb_queue*, ICBPtr);    /* append the icb to the end */
void   icb_remove(icb_queue*, ICBPtr);    /* remove the icb */
int    icb_length(icb_queue*);

#define que_not_empty(Q)    ((Q)->head)
#define que_empty(Q)        ((Q)->head == NULL)
#define que_almost_empty(Q) (que_empty(Q) || (Q)->head->next == NULL)

#define icb_lock(IP)         pthread_mutex_lock(&((IP)->lock))
#define icb_unlock(IP)       pthread_mutex_unlock(&((IP)->lock))

/** Job dependend macros. */
#define system_wait() {                                 \
    extern int twfj;                                    \
    extern pthread_mutex_t job_n_twfj_mutex;            \
    pthread_mutex_lock( &job_n_twfj_mutex );            \
    if( jobs == 0 ) {                                   \
        twfj++;                                         \
        pthread_mutex_unlock( &job_n_twfj_mutex );      \
        transport_signal_wait();                        \
        pthread_mutex_lock( &job_n_twfj_mutex );        \
        twfj--;                                         \
    }                                                   \
    pthread_mutex_unlock( &job_n_twfj_mutex );          \
}
#define system_signal() transport_signal();

#define dec_jobs() {                                    \
    extern int jobs;                                    \
    extern pthread_mutex_t job_n_twfj_mutex;            \
    pthread_mutex_lock( &job_n_twfj_mutex );            \
    jobs = (jobs)? jobs-1: 0;                           \
    pthread_mutex_unlock( &job_n_twfj_mutex );          \
}
#define inc_jobs() {                                    \
    extern int jobs;                                    \
    extern pthread_mutex_t job_n_twfj_mutex;            \
    pthread_mutex_lock( &job_n_twfj_mutex );            \
    jobs++;                                             \
    pthread_mutex_unlock( &job_n_twfj_mutex );          \
}
#define inc_jobs_s() {                                  \
    extern int jobs;                                    \
    extern int twfj;                                    \
    extern int thread_count;                            \
    extern pthread_mutex_t job_n_twfj_mutex;            \
    pthread_mutex_lock( &job_n_twfj_mutex );            \
    jobs++;                                             \
    if( twfj != 0 && (thread_count - twfj) < jobs )     \
	system_signal();                                \
    pthread_mutex_unlock( &job_n_twfj_mutex );          \
}


#define icb_get(Q, IP) {                                    \
    		pthread_mutex_lock(&((Q)->lock));           \
 		if(que_empty(Q)) (IP) = NULL;               \
    		else {                                      \
                   (IP) = icb_first(Q);                     \
                   dec_jobs();                              \
                   icb_lock(IP);                            \
                }                                           \
  		pthread_mutex_unlock(&((Q)->lock));         \
                }


#define icb_put_signal(Q, IP) {                                   \
   		pthread_mutex_lock(&((Q)->lock));                 \
    		icb_append(Q, IP);                                \
                icb_unlock(IP);                                   \
                inc_jobs_s();                                     \
  		pthread_mutex_unlock(&((Q)->lock));               \
                }

#define icb_put(Q, IP) {                                          \
   		pthread_mutex_lock(&((Q)->lock));                 \
    		icb_append(Q, IP);                                \
                icb_unlock(IP);                                   \
                inc_jobs();                                       \
  		pthread_mutex_unlock(&((Q)->lock));               \
                }

#define icb_put_q(Q, IP) {                                        \
   		pthread_mutex_lock(&((Q)->lock));                 \
    		icb_append(Q, IP);                                \
                icb_unlock(IP);                                   \
  		pthread_mutex_unlock(&((Q)->lock));               \
                }

#define release_waiting_msger()                                                   \
		que_lock(waiting_msger_queue);                                    \
	        while( que_not_empty(waiting_msger_queue) ) {                     \
                    ICBPtr IP = icb_first(waiting_msger_queue);                   \
		    icb_put_signal(engine_queue, IP);                             \
                }                                                                 \
		que_unlock(waiting_msger_queue);


#if 0
/** Old unused defines. */
#define icb_lock(IP)         pthread_mutex_lock(&((IP)->lock))
#define icb_unlock(IP)       pthread_mutex_unlock(&((IP)->lock))
#define icb_wait(Q)         pthread_cond_wait(&((Q)->cond), &((Q)->lock))
#define icb_signal(Q)       pthread_cond_signal(&((Q)->cond))

#define system_wait()      {                                 \
	pthread_mutex_lock(&(system_lock));                  \
	while( system_count == 0 && mlvm_stop == 0 )       \
	  pthread_cond_wait( &system_cond, &system_lock );   \
	system_count--;                                      \
	pthread_mutex_unlock(&system_lock);                  \
}
#define system_signal()    {                                 \
	pthread_mutex_lock(&(system_lock));                  \
	system_count++;                                      \
	if( system_count == 1 )                              \
           pthread_cond_broadcast( &system_cond );           \
	pthread_mutex_unlock(&system_lock);                  \
}
#define icb_wait_get(Q, IP) {                                      \
    		pthread_mutex_lock(&((Q)->lock));                  \
 		while (que_empty(Q))                               \
     	 	  pthread_cond_wait(&((Q)->cond), &((Q)->lock));   \
    		(IP) = icb_first(Q);                               \
                icb_lock(IP);                                      \
  		pthread_mutex_unlock(&((Q)->lock));                \
                }

#define log_alive(LOG, IP) {                                     \
		(IP)->log = (LOG) = log_new();                   \
		(LOG)->application = (IP)->application;          \
		strcpy((LOG)->name, (IP)->name);                 \
		(LOG)->status = LOG_ALIVE;                       \
		(LOG)->server = (IP)->queen;                     \
		(LOG)->ip = (IP);                                \
		(LOG)->jumps = (IP)->jumps;                      \
		(LOG)->time = time(NULL);                        \
		log_lock();                                      \
		log_append(LOG);                                 \
		log_unlock();                                    \
		}
/** End of old unuse defines. */
#endif		


 	/************************************************/
	/*	 	Error code                          */
	/************************************************/

typedef enum {
  E_SUCCEED,
  E_FILE_NOT_FOUND,
  E_ICB_OUT,
  E_ILLEGAL_ACCESS,
  E_ILLEGAL_ARGUMENT,
  E_ILLEGAL_BUILTIN,
  E_ILLEGAL_CLONE,
  E_ILLEGAL_CREATE,
  E_ILLEGAL_ENTRY,
  E_ILLEGAL_HOST,
  E_ILLEGAL_MESGER,
  E_ILLEGAL_MOVE,
  E_ILLEGAL_STATUS,
  E_ILLEGAL_THREAD,
  E_MEMORY_OUT,
  E_NO_SOLUTION,
  E_RESSOURCE_BUSY,
  E_UNKNOWN_IMAGO,
  E_UNKNOWN_OPCODE,
  E_UNKNOWN_TYPE,
} error_code;

int dispose_of( ICBPtr );
extern void system_error(char*, error_code);
extern void print_code(int, char*, int*, int*);
extern void print_proc_table(int, char*, int*, int*);
extern void print_term(int, char*, int);
extern void trace_control(ICBPtr, int, int);
extern void trace_call(ICBPtr, int*, int*, int);
extern void trace_return(ICBPtr, int);
extern int  search_index(char*, ICBPtr, int*);
extern int* copy_arg(int, char*, ICBPtr);
extern int  release_msger(ICBPtr, char*);

extern int mlvm_stop;

long int gethostid_cache(void);


#define name_ptr(BASE, INDEX) ((BASE) + (INDEX) - ASCII_SIZE)

#define adjust_code(ITYPE, IDIRECT, IADJUST, IHASH) {             \
	switch (ITYPE) {                                          \
          caseop(I_STATIONARY):                                   \
          caseop(I_WORKER):                                       \
          caseop(I_MESSENGER):                                    \
          caseop(I_E):                                            \
		IDIRECT;                                          \
		IADJUST;                                          \
		break;                                            \
	  caseop(I_O):                                            \
 	  	IDIRECT;                                          \
	  	break;                                            \
          caseop(I_N):                                            \
          caseop(I_C):                                            \
          caseop(I_F):                                            \
          caseop(I_B):                                            \
          caseop(I_I):                                            \
          caseop(I_P):                                            \
		IDIRECT;                                          \
		IDIRECT;                                          \
		break;                                            \
          caseop(I_R):                                            \
          caseop(I_2N):                                           \
          caseop(I_NC):                                           \
          caseop(I_NF):                                           \
          caseop(I_NI):                                           \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
  		break;                                            \
          caseop(I_3N):                                           \
          caseop(I_NR):                                           \
          caseop(I_2NF):                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
   		break;                                            \
          caseop(I_4N):                                           \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
   		break;                                            \
          caseop(I_5N):                                           \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
  		break;                                            \
          caseop(I_6N):                                           \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
  		break;                                            \
          caseop(I_7N):                                           \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
  		break;                                            \
          caseop(I_8N):                                           \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
   		break;                                            \
          caseop(I_NE):                                           \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IADJUST;                                          \
		break;                                            \
          caseop(I_2NE):                                          \
          caseop(I_NFE):                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IADJUST;                                          \
		break;                                            \
          caseop(I_2NFE):                                         \
          caseop(I_3NE):                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IADJUST;                                          \
		break;                                            \
          caseop(I_3NFE):                                         \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
 		IADJUST;                                          \
		break;                                            \
          caseop(I_4E):                                           \
		IDIRECT;                                          \
		IADJUST;                                          \
		IADJUST;                                          \
		IADJUST;                                          \
		IADJUST;                                          \
		break;                                            \
          caseop(I_N4E):                                          \
		IDIRECT;                                          \
		IDIRECT;                                          \
 		IADJUST;                                          \
		IADJUST;                                          \
		IADJUST;                                          \
		IADJUST;                                          \
		break;                                            \
          caseop(I_HTAB):		                                  \
 		IHASH;                                            \
   		break;                                            \
          default:                                                \
		printf("Something wrong during adjusting code entries\n");   \
	}                                                         \
     }

#endif
