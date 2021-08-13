/** $RCSfile: lib.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: lib.h,v 1.8 2003/03/25 21:05:08 hliang Exp $
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

#include <stdio.h>
#include <math.h>
/* some parameter  */
#define DEF_IMP_SUFFIX	".imp"
#define MAX_NAME_LEN	(256)
#define PRE_MALLOC_MEM (256000)
#define MAXVAR            (200)			/* per term or clause */
#define LINE_BUF	    (511)             /* has been defined in imagoscan.c */
#define  TRUE  (1)
#define  FALSE (0)
/* type of imago term */
#define VAR	(1)
#define UVAR	(2)
#define NAME	(3)
#define REAL	(4)
#define INTEGER	(5)
#define STR	(6)
#define PAREN	(7)
#define FUNC	(8)
#define ATOM	(9)
#define ENDFILE	(10)
#define PUNC	(11)
#define SYMRULEIFVAL  (12)            /* for type=":-"  */
#define SYMCOMMAVAL   (13)          /* for type=","  */
#define VOIDVAR  (14)
#define MLVMLIS  (15)
#define MLVMINT   (16)
#define MLVMOPERATOR  (17)
#define MLVMINTERMEDVAL (18)
#define MLVMDEEPCUT (19)
#define MLVMLVS  (20)
#define MLVMCOMPARISON  (21)
#define MLVMNECKCUT (22)
#define MLVMIFTHENELSE (23)
#define MLVMIFCOMPARISON (24)
#define MLVMTERMCOMPARISON (25)
#define MLVMNIL (26)       /* defind for optimization, original treated as ATOM */
#define MLVMATOMCOMP (27)      /* atom for comparison, need allocation */


#define deref(p) while (*(p) > 0) p = (int *) *(p);

FILE *cur_stream;

