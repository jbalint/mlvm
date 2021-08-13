/** $RCSfile: engine.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: engine.h,v 1.7 2003/03/24 16:15:28 gautran Exp $ 
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
	/*	 	engine.h                        */
	/*                                              */
	/*    Define macros used by the mlvm-engine:    */ 
	/*       pp;	// program pointer              */
	/*       af;	// active frame pointer         */
	/*       cp;	// continuation program pointer */
	/*       cf;	// continuation frame pointer   */
	/*       st;	// stack pointer                */
	/*       gp;	// successive get/set pointer   */
	/*       ax;	// register                     */
	/*       bx;	// register                     */
	/*       bb;	// backtrack frame pointer      */
	/*       b0;	// cut backtrack frame pointer  */
	/*       gl;	// generation line              */
	/*       tt;	// trail pointer                */
	/*       nl;	// nesting level                */
	/*       cx;	// register                     */
	/*       dx;	// register                     */
	/************************************************/
	
#ifndef ENGINE_HH
#define ENGINE_HH

#include "system.h"

enum {	TRACE_CALL	= 1,
	TRACE_LCALL	= 0,
	TRACE_RETURN	= 1,
	TRACE_PROCEED	= 0
     };

	/************************************************/
	/*      control and arithmetic macros           */
	/************************************************/


#define ACF     (*(af))
#define ACP     (*(af - 1))
#define ATT     (*(af - 2))
 
#define CCF     (*(cf))
#define CCP     (*(cf - 1))
#define CTT     (*(cf - 2))

#define BCF     (*(bb))
#define BCP     (*(bb - 1))
#define BTT     (*(bb - 2))
#define BBB     (*(bb - 3))
#define BB0     (*(bb - 4))
#define BBP     (*(bb - 5))
#define BGL     (*(bb - 6))

#define OD1     (*(pp + 1))
#define OD2     (*(pp + 2))
#define OD3     (*(pp + 3))
#define OD4     (*(pp + 4))
#define OD5     (*(pp + 5))
#define OD6     (*(pp + 6))
#define OD7     (*(pp + 7))
#define OD8     (*(pp + 8))
#define LAB1    (*(pp + 1))
#define LAB2    (*(pp + 2))
#define LAB3    (*(pp + 3))
#define LAB4    (*(pp + 4))
#define LAB5    (*(pp + 5))

#define cell(N)     (af - N)

#define next(X)     { pp += (X); goto CONTROL; }

#define jump(X)     { pp = (X); goto CONTROL; }

#define context_switch(T, E) {                                             \
                      plt->status = (T);                                   \
                      plt->nl = (E);                                       \
                      goto CONTEXIMAGO_SWITCH;                             \
                    }

#define derefstr(X, Y) {                                                      \
	              (X) = (int)(Y);                                         \
		      while (is_str(*addr((X))) && (X) != *(int*)(addr((X)))) \
			  X = *(int*) (addr(X));                              \
                    }

#define deref(X, Y) {                                                      \
                       X = *(int*) (Y);                                    \
                       while (is_var(X) && X != *(int*)(X))                \
                          X = *(int*) (X);                                 \
                    }
				
#define backtrack() {                                                      \
                       if (bb <= plt->stack_base){                         \
                          context_switch(IMAGO_TERMINATE, E_NO_SOLUTION);  \
                       }                                                   \
                       else if( bt-- < 0 ) {                               \
                          b0 = BB0;                                        \
                          pp = (BBP);                                      \
                          debug("*** Forced Context switch Point. ***\n"); \
                          context_switch(IMAGO_READY, E_SUCCEED);          \
                       } else {                                            \
                          b0 = BB0;                                        \
                          jump(BBP);                                       \
                       }                                                   \
                     }
			
#define trail(X)   if ((int*)(X) <= gl) { *tt-- = (int*)(X); }

#define bbtrail(X) if ((int*)(X) < bb) { *tt-- = (int*)(X); }

#define detrail(X)  while (tt < (int**)(X)){                               \
                       gp = *(++tt);                                       \
                       if (is_var((int)gp) && (gp < bb))                   \
                          *gp = (int) gp;                                  \
                    }

#define new_generation()  { *tt-- = (int*) make_str(gl); }

#define make_aframe() {                                                    \
                     af = st = st + OD2;                                   \
                     ACF = cf;                                             \
                     ATT = tt;                                             \
                    }

#define make_dframe() af = st = st + OD2;

	/************************************************/
	/* make_cframe: allocate a choice frame and     */
	/* initialize it; set a new nondeterministic    */
	/* generation; mirror calling arguments on the  */
	/* top of the stack.                            */
	/************************************************/
	
#define make_cframe() {                                                    \
                      ax = (int) bb;                                       \
                      bb = st + CHOICE_SIZE;                               \
                      BCF = cf;                                            \
                      BCP = CCP;                                           \
                      BTT = tt;                                            \
                      BBB = (int*) ax;                                     \
                      BB0 = b0;                                            \
                      BBP = LAB2;                                          \
                      BGL = gl;                                            \
                      ax = OD1;                                            \
                      af = st - ax;                                        \
                      gl = st = bb;                                        \
                      new_generation();                                    \
                      while (ax--) *(++st) = *(++af);                      \
                      af = st;                                             \
                    }
	

#define filter_trail() while (tt > (int**) gp){                            \
                        if (*tt <= gl) tt--;                               \
                        else  *tt = (int*) *(++gp);                        \
                    }

#define clear_trail()  while (tt > (int**) gp) {                           \
                           gp++; **(int**)(gp) = *gp;                      \
                       }

				
#define adjust_st(X)   st += (X)

#define ari_cpu(OP) {                                                      \
          deref(ax, cell(OD1));                                            \
          deref(bx, cell(OD2));                                            \
          if (is_int(ax)){                                                 \
            if (is_int(bx)){                                               \
               *cell(OD3) = make_int(int_value(ax) OP int_value(bx));      \
               next(4);                                                    \
            }                                                              \
            else if (is_float(bx)){                                        \
               cx = (int) cell(OD3);                                       \
               *(int*) cx = make_float((double)int_value(ax)               \
                                       OP float_value(bx), st);            \
               adjust_st(3);                                               \
               trail(cx);                                                \
               next(4);                                                    \
            }                                                              \
          }                                                                \
          else if (is_float(ax)){                                          \
            if (is_int(bx)){                                               \
               cx = (int) cell(OD3);                                       \
              *(int*) cx = make_float(float_value(ax)                      \
                                       OP (double)int_value(bx), st);      \
               adjust_st(3);                                               \
               trail(cx);                                                \
               next(4);                                                    \
            }                                                              \
            else if (is_float(bx)){                                        \
               cx = (int) cell(OD3);                                       \
              *(int*) cx = make_float(float_value(ax)                      \
                                       OP float_value(bx), st);            \
               adjust_st(3);                                               \
               trail(cx);                                                \
               next(4);                                                    \
            }                                                              \
          }                                                                \
          context_switch(IMAGO_ERROR, E_UNKNOWN_TYPE);                     \
        }
 	
#define int_cpu(OP) {                                                      \
           deref(ax, cell(OD1));                                           \
           deref(bx, cell(OD2));                                           \
           if (is_int(ax) && is_int(bx)){                                  \
              *cell(OD3) = make_int(int_value(ax) OP int_value(bx));       \
              next(4);                                                     \
           }                                                               \
           context_switch(IMAGO_ERROR, E_UNKNOWN_TYPE);                    \
         }

#define int_one(OP) {                                                      \
           deref(ax, cell(OD1));                                           \
           if (is_int(ax)){                                                \
             *cell(OD2) = ax OP MLVM_NOTAG_ONE;                            \
             next(3);                                                      \
           }                                                               \
           context_switch(IMAGO_ERROR, E_UNKNOWN_TYPE);                    \
         }
			
#define ari_cmp(OP) {                                                      \
           deref(ax, cell(OD1));                                           \
           deref(bx, cell(OD2));                                           \
           if (is_int(ax)){                                                \
             if (is_int(bx)){                                              \
               if (ax OP bx){                                              \
                 next(4);                                                  \
               }                                                           \
               else {                                                      \
                 jump(LAB3);                                               \
               }                                                           \
             }                                                             \
             else if (is_float(bx)){                                       \
               if ((double) int_value(ax) OP float_value(bx)){             \
                 next(4);                                                  \
               }                                                           \
               else{                                                       \
                 jump(LAB3);                                               \
               }                                                           \
             }                                                             \
           }                                                               \
           else if (is_float(ax)){                                         \
             if (is_int(bx)){                                              \
               if (float_value(ax) OP (double) int_value(bx)){             \
                 next(4);                                                  \
               }                                                           \
               else {                                                      \
                 jump(LAB3);                                               \
               }                                                           \
             }                                                             \
             else if (is_float(bx)){                                       \
               if (float_value(ax) OP float_value(bx)){                    \
                 next(4);                                                  \
               }                                                           \
               else{                                                       \
                 jump(LAB3);                                               \
               }                                                           \
             }                                                             \
           }                                                               \
           context_switch(IMAGO_ERROR, E_UNKNOWN_TYPE);                    \
         }

#endif
