/** $RCSfile: frame.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: frame.h,v 1.9 2003/03/24 14:07:48 gautran Exp $ 
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

#ifndef __FRAME_H_
#define __FRAME_H_

#include "queue.h"

/** How much a frame may contain. */
#ifndef _FRAME_SIZE
# define FRAME_SIZE 4096
#else 
# define FRAME_SIZE _FRAME_SIZE
#endif

/** The dynamic frame expansion follow an hysteresis curve.
    A mid point, a upper limit and lower limit are defined 
    and appart by a window constant.
    If the frame allocation crosses the upper threshold, 
    the three variables are incremented by the amount of the window.
    If the frame deallocation crosses the lower threshold,
    the three variables are decremented by the amount of the window.
    Frames below the mid point are not deallocated at destroy time.
    The result is that the frame allocation will be held within the lower
    half of the window such that less malloc are performed. */
/** The dynamic frame allocation window. */
#ifndef _FRAME_COUNT_WINDOW
# define FRAME_COUNT_WINDOW 1024
#else 
# define FRAME_COUNT_WINDOW _FRAME_COUNT_WINDOW
#endif


/** How many frames do we want to have at most. */
//#ifndef _FRAME_COUNT_MAX
//# define FRAME_COUNT_MAX 8192
//#else 
//# define FRAME_COUNT_MAX _FRAME_COUNT_MAX
//#endif


/** How many frames do we want to have at least. */
//#ifndef _FRAME_COUNT_MIN
//# define FRAME_COUNT_MIN 1024
//#else 
//# define FRAME_COUNT_MIN _FRAME_COUNT_MIN
//#endif

/** Frame structure declaration.
    A frame is like a stack, you stack your element in a first in last out order.
*/
struct frame_str {
  struct link_str link; /** Links all frame together. */
  unsigned short top; /** Index of the first byte in the frame. */  
  unsigned char fc[FRAME_SIZE];
};

void frame_init( );
void frame_cleanup( );


void frame_destroy( struct link_head_str * );
int  frame_size( const struct link_head_str * ); 
int  frame_count( const struct link_head_str * ); 
int  frame_send( int, struct link_head_str * ); 
int  frame_erase( struct link_head_str *, const int ); 

int  frame_move( struct link_head_str *, struct link_head_str *, const int ); 
int  frame_copy( struct link_head_str *, const struct link_head_str *, const int ); 
int  frame_append_string( register struct link_head_str *, const char *, const int );
int  frame_peek_string( register struct link_head_str *, char *, const int );

int  frame_write_string( register struct link_head_str *, const char *, const int );
int  frame_write_text( register struct link_head_str *, const char * );
int  frame_write_int( register struct link_head_str *, int );
int  frame_write_short( register struct link_head_str *, short );
int  frame_write_byte( register struct link_head_str *, char );
int  frame_read_string( register struct link_head_str *, char *, const int );
int  frame_read_text( register struct link_head_str *, char *, int );
int  frame_read_int( register struct link_head_str *, int * );
int  frame_read_short( register struct link_head_str *, short * );
int  frame_read_byte( register struct link_head_str *, char * );


#endif
