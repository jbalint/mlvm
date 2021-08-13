/** $RCSfile: version.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: version.h,v 1.9 2003/03/25 23:31:14 gautran Exp $ 
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
#ifndef __VERSION_H_
#define __VERSION_H_

#define MLVM_MAJOR      0
#define MLVM_MINOR      1
#define MLVM_REV        B
#define MLVM_REVNUM     1

#define MLVM_VERSION    "0.1B1"
#define MLVM_NAME       "MLVM"
#define MLVM_COPYRIGHT  "Copyright 2003: IMAGO LAB (G. Autran & Dr. X. Li)\n  University of Guelph, Ontario, Canada\n"

#define MLVMC_VERSION    "0.1"
#define MLVMC_NAME       "MLVMC"

#ifndef _BUILD_DATE
# define MLVM_BUILDDATE 
#else
# define MLVM_BUILDDATE _BUILD_DATE
#endif
#define MLVM_EXT_VERSION MLVM_VERSION "(" MLVM_BUILDDATE ")"

#endif
