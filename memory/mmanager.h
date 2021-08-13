/** $RCSfile: mmanager.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: mmanager.h,v 1.5 2003/03/21 13:12:03 gautran Exp $ 
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
	/*	 	mmanager.h			*/
	/************************************************/

#ifndef MMANAGER_HH
#define MMANAGER_HH


#define		MM_TURNING		5
#define		MM_EXP_SIZE		1048576	

/** Size of the memory manager static buffer. */
#define         GC_STATIC               GC_LIMIT*2


/****************************************************************/
/*			Macros (memory expansion)		*/
/****************************************************************/

#define mtype()	(opcode_tab[*tp2].type)

#define mdirect()	{tp2++;}

#define madjust()	{(int*) *tp2++ += distance;}

#define mhash()		{tp2++;                            \
			ax = *tp2 + 1;                     \
			tp2++;	                           \
			while (ax--){                      \
				(int*) *tp2 += distance;       \
				tp2 += 2;                      \
			}                                  \
			tp2--;                             \
			}
		 
/****************************************************************/
/*			Macros (GARBAGE COLLECTION)		*/
/****************************************************************/

#define dest(X)		(int)(cline + ((int*)(X) - copy_bottom))

#define gc_pop()	(tt == (int**)scanp) ? 0 : (int)*(++tt)

#define gc_push(X)	*tt-- = (int*) (X)
     
#define hit_old(X)	(((int*)(X) < cline) && ((int*) (X) >= stack_bottom))

// hit_mid should consider generation lines in root set
// a generation line has a STR tag so must be compared by (void*)

#define hit_mid(X) (((void*)(X) <= (void*) st) && ((void*)(X) >= (void*)cline))

#define hit_previous(X)	(((int*)(X) < prevp) && ((int*) (X) >= stack_bottom))


#define hit_copied_var(X) (((int*) (X) < freep) && ((int*)(X) >= copy_bottom))
 
#endif
