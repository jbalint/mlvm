/** $RCSfile: pdu_backup.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: pdu_backup.h,v 1.3 2003/03/24 14:07:48 gautran Exp $ 
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

#ifndef __PDU_BACKUP_H_
#define __PDU__BACKUP_H_

void backup_sync();

/** Imago PDU backup function (used by the ADA rendez-vous, and security layers). */
int backup_save( const char *, const struct pdu_str * );
struct pdu_str *backup_load( const char *, struct pdu_str * );
struct pdu_str *backup_idload( const int, struct pdu_str * );
int backup_cancel( const char * );
int backup_idcancel( const int );

#define BACKUP_NAME_MAX  MAX_DNS_NAME_SIZE + 70
/** The size of the cache table for the backup cache. DO NOT CHANGE IT. */
#define MAX_PDU_BACKUP_CACHE_ENTRY 256

/** Use the new caching algorithm or not... */
//#define _PDU_BACKUP_USE_CACHE

#endif
