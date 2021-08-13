/** $RCSfile: version.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: version.h,v 1.3 2003/03/21 13:12:03 gautran Exp $ 
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

#ifndef __IMAGO_VERSION_H_
#define __IMAGO_VERSION_H_

#define IMAGO_TRANSPORT_MAJOR_SHIFT   8

#define IMAGO_TRANSPORT_SICP_MAJOR    1
#define IMAGO_TRANSPORT_SICP_MINOR    2
#define IMAGO_TRANSPORT_SICP_VERSION  (IMAGO_TRANSPORT_SICP_MAJOR << 8 | IMAGO_TRANSPORT_SICP_MINOR)

#define IMAGO_TRANSPORT_SITP_MAJOR    1
#define IMAGO_TRANSPORT_SITP_MINOR    2
#define IMAGO_TRANSPORT_SITP_VERSION  (IMAGO_TRANSPORT_SITP_MAJOR << 8 | IMAGO_TRANSPORT_SITP_MINOR)

#define IMAGO_R_VERSION    "0.1"


#endif
