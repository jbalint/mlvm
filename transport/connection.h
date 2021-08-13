/** $RCSfile: connection.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: connection.h,v 1.3 2003/03/21 13:12:03 gautran Exp $ 
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

#ifndef __CONNECTION_H_
#define __CONNECTION_H_


/** Connection timeout. */
#define CLEANUP_PERIOD 2 MINS
#define CONNECTION_TIMEOUT_SEC  15 MINS


/** The connection status may be one of the following. */
typedef enum {
    C_FREE = 0,
    C_ACTIVE,
    C_LOCKED
} status_t;

/** Local to this layer data structure. */
typedef struct con_str {
    status_t status;
    int fd;
    struct sockaddr_in peer;
    struct sockaddr_in local;
    struct link_head_str frames;
    time_t timestamp;
#ifdef _REENTRANT
    pthread_mutex_t lock;
    pthread_cond_t l_cv;
#endif
} con_t;

int con_init();
void con_end();
int con_trylock( con_t * );
int con_lock( con_t *  );
int con_unlock( con_t * );
void con_free( con_t *  );
con_t *con_new(struct sockaddr_in *);
con_t *lookup_con( struct sockaddr_in * );
con_t *make_con( con_t *, struct sockaddr_in *, pid_t );
int con_rcv( con_t * );
con_t *con_next( con_t * );



/** Some macro helper. */
#define get_csocket(C) (((C) != NULL)? (C)->fd: -1)
#define set_csocket(C,S) if((C) != NULL) (C)->fd = (S);
#define get_raddr(C, A) if((C) != NULL) memcpy((A), &(C)->peer, sizeof((C)->peer));
#define get_laddr(C, A) if((C) != NULL) memcpy((A), &(C)->local, sizeof((C)->local));
#define get_avail(C) (((C) != NULL)? frame_size(&(C)->frames): 0)
#define con_peek(C, B, L) (((C) != NULL)? frame_peek_string(&(C)->frames, (B), (L)): 0)
#define con_read(C, F, L) (((C) != NULL)? frame_move((F), &(C)->frames, (L)): 0)
#define con_settimeout(T) con_timeout_sec = (T)
#define con_update(C) time(&((con_t*)(C))->timestamp);


extern inline int con_test(con_t *C);

/** */
/** Extern variables. */
extern int con_timeout_sec;

#endif
