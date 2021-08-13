/** $RCSfile: frame.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: frame.c,v 1.12 2003/03/24 14:07:48 gautran Exp $ 
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

/** This file contains the framing mechanism function in order to avoid dynamic
    memory allocation of the PDUs. 
    The mechanism is quite simple, a linked list of blocks of memory (called "frame")
    is allocated at the initialization time, then, everytime somebody wants to write
    some data into the PDU, a frame is brought from the list, the data is stored in
    this frame, and the frame is stored at the end of the PDU control block frame list.
    all the previous frames must be used before a new one allocated.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "include.h"
#include "frame.h"

/** This is the head of the free frames list. */
struct link_head_str free_frame;
/** Some counters stuff. */
//static int frame_count_max = FRAME_COUNT_MAX; /** Can be re-adjusted to allow big imago to get through. */
//const static int frame_count_min = FRAME_COUNT_MIN; /** Never change. */
#define FRAME_HALF_WINDOW FRAME_COUNT_WINDOW/2

static unsigned long frame_upper_threshold = 2 * FRAME_HALF_WINDOW;
//static unsigned long frame_mid_point       = FRAME_HALF_WINDOW;
static unsigned long frame_lower_threshold = 0;

static unsigned long counter = 0;
#ifdef _REENTRANT
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER ;
#endif



struct frame_str *frame_new() {
    /** Get a free frame. */
    struct frame_str *nf = (struct frame_str *)dequeue( &free_frame );
    /** If we ran out of frames, try to create one more. */
    if( nf == NULL ) {
	if( (nf = (struct frame_str *)malloc( sizeof(struct frame_str))) != NULL ) {
#ifdef _REENTRANT
	    pthread_mutex_lock( &counter_lock );
#endif
	    counter++;
	    /** Ok, we can try to dynamically re-adjust the upper limit because our 
		malloc succeed... */
	    if( counter > frame_upper_threshold ) {
		/** If we successfully allocated more than the upper threashold, 
		    we will slide the window up because it is worth it. */
		frame_upper_threshold += FRAME_HALF_WINDOW;
		frame_lower_threshold += FRAME_HALF_WINDOW;
		debug( "Re-adjusting maximum frame limit to fit the system (low: %ld, count: %ld, high: %ld).\n",
			frame_lower_threshold, counter, frame_upper_threshold );
	    }
#ifdef _LOG_STATISTICS
	    debug( "Creating a new frame (min: %d, current: %d, max: %d)\n",
		   frame_lower_threshold, counter, frame_upper_threshold );
#endif
#ifdef _REENTRANT
	    pthread_mutex_unlock( &counter_lock );
#endif
	} else {
	    warning( "Unable to create new frame: System low in memory.\n" );
	}
    } else {
#ifdef _REENTRANT
	pthread_mutex_lock( &counter_lock );
#endif
	counter++;
#ifdef _LOG_STATISTICS
	debug( "Creating a new frame (from the pool)\n" );
#endif
#ifdef _REENTRANT
	pthread_mutex_unlock( &counter_lock );
#endif
    }
    /** Clean the frame for security reasons. */
    if( nf ) {
	memset( nf, 0, sizeof(struct frame_str) );
	/** Reset the top index and bottom index. */
	nf->top    = FRAME_SIZE-1;
    }
    /** And return what's left of it... */
    return nf;
}
void frame_free( struct frame_str *ff ) {
    /** The frame to clear most be of any list... otherwise, we do not guarantee the success (only a core dump) */
    /** Keep the block of memory. */
    enqueue( &free_frame, &ff->link );
#ifdef _REENTRANT
    pthread_mutex_lock( &counter_lock );
#endif
    counter--;
    /** If the number of frames in the system goes below the lower threshold,
	we need to de-allocate the extra frames. */
    if( counter < frame_lower_threshold ) {
	int c, extra;
	/** get the number of frames in the pool. */
	c = frame_count(&free_frame);
	/** Then re-adjust the thresholds. */
	/** If the lower threshold has been crossed, we will slide the window down a bit
	    because it is worth it. 
	    Make sure we do not fall below the 0 limit.*/
	if( frame_lower_threshold >= FRAME_HALF_WINDOW ) {
	    frame_upper_threshold -= FRAME_HALF_WINDOW;
	    frame_lower_threshold -= FRAME_HALF_WINDOW;
	} else {
	    frame_upper_threshold = 2*FRAME_HALF_WINDOW;
	    frame_lower_threshold = 0;
	}
	debug( "Re-adjusting maximum frame limit to fit the system (low: %ld, count: %ld, high: %ld).\n",
		frame_lower_threshold, counter, frame_upper_threshold );
	/** Then, free the extra frames. */
	extra = (c + counter) - frame_upper_threshold;
	debug( "Freeing %d frame(s).\n", extra );
	for( ; extra > 0; extra-- ) {
	    ff = (struct frame_str *)dequeue( &free_frame );
	    if( ff )
		free(ff);
	    else
		break;
	}
    } else {
#ifdef _LOG_STATISTICS
	debug( "Freeing a frame (back in the pool)\n" );
#endif
    }
#ifdef _REENTRANT
    pthread_mutex_unlock( &counter_lock );
#endif
}
void frame_init( ){
    /** 
     * TO DO: Create a minimum number of frames. 
     */
}
void frame_cleanup( ){
    struct frame_str *ff;
    debug( "Cleaning up all frames in the system.\n" );
    do {
	ff = (struct frame_str *)dequeue( &free_frame );
	if( ff ) {
	    /** Clear the block. */
	    free( ff );
#ifdef _REENTRANT
	    pthread_mutex_lock( &counter_lock );
#endif
	    counter--;
#ifdef _REENTRANT
	    pthread_mutex_unlock( &counter_lock );
#endif
	}
    } while( ff );
}

void frame_destroy( struct link_head_str *lt ){
    register struct frame_str *_f;
    /** As long as there is frames in this list... */
    while( (_f = (struct frame_str *)dequeue(lt)) != NULL ) {
	frame_free( _f );
    }
}

/** Erase as many data from the front of the frame as the length requests.
    And return the amount of the removed data. */
int  frame_erase( register struct link_head_str *lt, const int length ) {
    /** Scan through the list of frame to find the total size... */
    register int s = length, e = 0;
    register struct frame_str *cf;
    /** Main loop. */
    do {
	/** Grab the first frame. */
	if( (cf = (struct frame_str *)first( lt )) == NULL )
	    break;
	e = FRAME_SIZE - (cf->top+1);
	if( s >= e ) {
	    /** This is an empty frame. */
	    pop( lt );
	    frame_free( cf );
	} else {
	    /** Partial frame. */
	    cf->top += s;
	    e = s;
	}
	s -= e;
    } while( s > 0 );
    /** Most of it down. */
    return (length - s);
}

int frame_count( const struct link_head_str *_h ) {
    /** Return the number of frames in this list. */
    int s = 0;
    register const struct link_str *_l;
    /** We need to lock the all thing... */
#ifdef _REENTRANT
    pthread_mutex_lock( &((struct link_head_str*)_h)->lock );
    {
#endif
	for( s = 0, _l = _h->first; _l; _l = _l->next ) 
	    s ++;
#ifdef _REENTRANT
    }
    pthread_mutex_unlock( &((struct link_head_str*)_h)->lock );
#endif
    
    /** Return the total. */
    return s;
}

int frame_size( const struct link_head_str *_h ) {
    /** Scan through the list of frame to find the total size... */
    int s = 0;
    register const struct link_str *_l;
    /** We need to lock the all thing... */
#ifdef _REENTRANT
    pthread_mutex_lock( &((struct link_head_str*)_h)->lock );
    {
#endif
	for( s = 0, _l = _h->first; _l; _l = _l->next ) {
	    s += FRAME_SIZE - (((struct frame_str *)_l)->top+1);
	}
#ifdef _REENTRANT
    }
    pthread_mutex_unlock( &((struct link_head_str*)_h)->lock );
#endif

    /** Return the total. */
    return s;
}

/** In this function, if the application is multi-thread, we do not worry about someone 
    modifying the frame list for this PDU because this PDU must have been locked already
*/
int  frame_send( int sd, struct link_head_str *lt ) {
    register struct frame_str *_f = NULL;
    register int s = 0, s1 = 0;
    register unsigned char *ax = 0, *bx = 0;

    /** We will send all frames to the network, 
	and we assume somebody will worry about freeing them off. */
    for( _f = (struct frame_str *)first( lt ); _f; _f = (struct frame_str *)next( lt, &_f->link ) ) {    
	ax = &_f->fc[_f->top+1];
	bx = &_f->fc[FRAME_SIZE];
	do {
	    if( (s1 = write(sd, ax, bx-ax)) < 0 ) {
		/** An error occured. */
		//	warning( "We are going to fast for that guy.\n" );
		return s;
	    }
	    ax += s1;
	    s += s1;
	} while( ax < bx );
    }
    return s;
}

int  frame_append_string( register struct link_head_str *lt, const char *st, const int length ) {
    struct link_head_str ext_f;
    register struct link_str *link;
    /** Initialize the link header. */
    queue_init( &ext_f );
    /** ! ! ! WARNING ! ! ! */
#warning "Function not optimized for small chunk of data."
    /**
     *  There is no frame optimization and completion, so do not use small chunk of data but
     *  rather big chunk of data, about the size of the frame.
     */ 
    if( frame_write_string( &ext_f, st, length ) < 0 ) 
	return -1;
  
    for( link = pop( &ext_f ); link; link = pop( &ext_f ) ) {
	enqueue( lt, link );
    }
    return length;
}
int frame_copy( struct link_head_str *dlt, const struct link_head_str *slt, const int l ) {
    register int l1 = l, l2;
    register struct frame_str *cf = NULL;
    /** We copy data from one frame set to another one. */
    for( cf = (struct frame_str *)first( (struct link_head_str *)slt ); 
	 (cf != NULL) && (l1 > 0); 
	 cf = (struct frame_str *)next( (struct link_head_str *)slt, &cf->link ) ) {
	l2 = (FRAME_SIZE - (cf->top+1));
	l2 = (l1 < l2) ? l1: l2;
	if( (l2 <= 0) || ((l2 = frame_append_string(dlt, &cf->fc[cf->top+1], l2)) < 0) )
	    break;
	l1 -= l2;
    }
    return l - l1;
}

int frame_move( struct link_head_str *dlt, struct link_head_str *slt, const int l ) {
    register int l1 = l, i;
    register struct frame_str *cf;
    register struct frame_str *exf = frame_new(); /** Worst case (most cases) we will need one extra frame. */
    if( exf == NULL ) {
	return -1;
    }
    /** To move data from one frame list to another, we just move the frames, then
	for the last frame, we move only the data. */
    do {
	cf = (struct frame_str *)first( slt );
	if( cf == NULL ) break;
	if( l1 >= (FRAME_SIZE - (cf->top+1)) ) {
	    /** Move the frame. */
	    pop(slt);
	    enqueue( dlt, (struct link_str *)cf );
	    l1 -= (FRAME_SIZE - (cf->top+1));
	} else if( l1 > 0 ) {
	    exf->top -= l1;
	    i = exf->top+1;
	    while( l1 ) {
		exf->fc[i++] = cf->fc[++cf->top];
		l1--;
	    }
	    enqueue(dlt, (struct link_str *)exf);
	    exf = NULL;
	} 
    } while( l1 );
    if( exf ) 
	frame_free( exf );
    return l - l1;
}

int  frame_peek_string( register struct link_head_str *lt, char *dt1, const int length ) {
    register struct frame_str *cf = (struct frame_str *)first( lt );
    register int c = (cf)? cf->top+1: 0;
    register int l1 = length;
    register char *_d = dt1;

    for( l1 = length; l1 && cf != NULL; c++ ) {
	if( c == FRAME_SIZE) {
	    /** Go to the next frame. */
	    cf = (struct frame_str *)next( lt, (struct link_str *)cf );
	    c = cf->top;
	    continue;
	}
	*_d = cf->fc[c];
	l1--;
	_d++;
    }
    return length - l1;
}

/** these functions are used to read/write a string/int/short/byte to the PDU header. */
/** The hard part is to put the item in the reverse order like in a stack... */
int frame_write_string( register struct link_head_str *lt, const char *st1, const int length ) {
    register const char *_b = st1;
    register const char *_e = st1+length;
    register struct frame_str *cf;

    do {
	/** Get the first frame in this list. */
	cf = (struct frame_str *)first( lt );
	if( cf == NULL || cf->top <= 0 ) {
	    /** Allocate one more frame. */
	    if( (cf = frame_new()) == NULL ) {
		/** we need to undo what we have done. */
		goto undo;
	    }
	    /** Stuff it on top of the other ones. */
	    push( lt, (struct link_str *)cf );
	}
	/**
	 * TO DO: Optimize move and copy long after long instead of bytes. 
	 */
	/** Start putting the bytes one before the others. */
	while( _b < _e && cf->top ) 
	    cf->fc[cf->top--] = *(--_e); 
	/** Then continue for the remaining bytes. */
    } while( _b < _e );
    /** Return how much we wrote. */
    return length;
    /** Cannot do that... */
 undo:
    /** Switch some pointers arround. */
    _b = _e;
    _e = st1+length;
    do {
	/** Grab the first frame. */
	if( (cf = (struct frame_str *)first( lt )) == NULL )
	    break;
	/** Discard as much as we can. */
	if( (_e - _b) < (FRAME_SIZE - cf->top) ) {
	    /** It all is in this frame. */
	    cf->top += (_e - _b);
	    /** Update the pointer. */
	    _b = _e;
	} else {
	    /** This entire frame is to be discarded. */
	    /** Update the pointer first. */
	    _b += (FRAME_SIZE - cf->top);
	    /** Get rid of the frame. */
	    pop( lt );
	    frame_free( cf );
	}
    } while( _b != _e );
    /** Signal the error. */
    return -1;

}
int frame_write_text( register struct link_head_str *lt, const char *te1 ) {
    /** Write the text + 1 for the zero */
    return frame_write_string( lt, te1, strlen(te1)+1 );
}
int frame_write_int( register struct link_head_str *lt, int v1 ) {
    /** Switch to network byte order. */
    v1 = htonl(v1);
    /** Then write it down. */
    return frame_write_string( lt, (char *)&v1, sizeof(int) );
}
int frame_write_short( register struct link_head_str *lt, short s1 ) {
    /** Switch to network byte order. */
    s1 = htons(s1);
    /** Then write it down. */
    return frame_write_string( lt, (char *)&s1, sizeof(short) );
}

/** Add one byte to the end of the frame. */
int frame_write_byte( struct link_head_str *lt, char c1 ) {
    /** write it down. */
    return frame_write_string( lt, (char *)&c1, sizeof(char) );
}

/**
 *  And now the reading part of it. 
 */
int frame_read_string_aux( register struct link_head_str *lt, char *st1, const int length, int flag ) {
    register struct frame_str *cf;
    register char *_d = st1;
    register int l1 = length;
    char stop = 0;
    do {
	/** Grab the first frame. */
	if( (cf = (struct frame_str *)first( lt )) == NULL )
	    break;
	/**
	 * TO DO: Optimize move and copy long after long instead of bytes. 
	 */
	/** Read as much as we can out of it. */ 
	for( ; ((cf->top+1) < FRAME_SIZE) && (l1 > 0); _d++, l1-- ){ 
	    *_d = cf->fc[++cf->top];
	    if( flag && !*_d ) {
		stop = 1;
		break;
	    }
	}
	/** Discard the frame if it is not needed anymore. */
	if( (cf->top+1) >= FRAME_SIZE ) {
	    /** This is an empty frame. */
	    pop( lt );
	    frame_free( cf );
	}
	if( flag && stop )
	    break;
    } while( l1 > 0 );
    /** Return how much we read. */
    return (length - l1);
}
int frame_read_string( register struct link_head_str *lt, char *st1, const int length ) {
    return frame_read_string_aux( lt, st1, length, 0 );
}

int frame_read_text( register struct link_head_str *lt, char *te1, int max ) {
    return frame_read_string_aux( lt, te1, max, 1 );
}

int frame_read_int( register struct link_head_str *lt, int *v1 ) {
    /** Read the integer. */
    if( frame_read_string(lt, (char *)v1, sizeof(int)) != sizeof(int) )
	return -1;
    /** Change the byte ordering. */
    *v1 = ntohl( *v1 );
    return 0;
}

int frame_read_short( register struct link_head_str *lt, short *s1 ) {
    /** Read the integer. */
    if( frame_read_string(lt, (char *)s1, sizeof(short)) != sizeof(short) )
	return -1;
    /** Change the byte ordering. */
    *s1 = ntohs( *s1 );
    return 0;
}

/** Get the first byte from the first frame. */
int frame_read_byte( struct link_head_str *lt, char *c1 ) {
    /** Read the byte. */
    if( frame_read_string(lt, (char *)c1, sizeof(char)) != sizeof(char) )
	return -1;
    return 0;
}

/** */
    
