/** $RCSfile: imago_pdu.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_pdu.h,v 1.11 2003/03/24 14:07:48 gautran Exp $ 
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

#include <sys/socket.h>
#include <netinet/in.h>

#include "queue.h"
#include "state_machine.h"
#include "imago_icb.h"
#include "timer.h"

#ifndef __IMAGO_PDU_H_
#define __IMAGO_PDU_H_


#define PDU_FLAG_RESERVED   0x00ff

#define PDU_TYPE_SITP       0x8000 /** If this flag is not present, it is a SICP. */
#define PDU_FLAG_2XXX       0x4000
#define PDU_FLAG_3XXX       0x2000
#define PDU_FLAG_4XXX       0x1000

#define PDU_FLAG_X1XX       0x0800
#define PDU_FLAG_X2XX       0x0400
#define PDU_FLAG_X3XX       0x0200
#define PDU_FLAG_X4XX       0x0100

#define PDU_FLAG_XX1X       0x0080
#define PDU_FLAG_XX2X       0x0040
#define PDU_FLAG_XX3X       0x0020
#define PDU_ERROR           0x0010

#define PDU_FLAG_XXX1       0x0008
#define PDU_FLAG_XXX2       0x0004
#define PDU_FLAG_FORWARDING 0x0002
#define PDU_FLAG_INCOMING   0x0001

typedef enum {
    EPDU_SUCCESS = 0,
    EPDU_MFRAME,         /** To many frames. */ 
    EPDU_MCONN,          /** To many connections. */
    EPDU_NFILE,          /** Not enough kernel memory to allocate a new socket structure. */
    EPDU_CONNREFUSED,    /** No one listening on the remote address. */
    EPDU_HOST_NOT_FOUND, /** The routing layer could not forward the PDU. */
    EPDU_NBACK,          /** PDU backup not possible. */
    EPDU_FOPEN,          /** A file could not be open. */        
    EPDU_FWRITE,         /** Error while writing to a file. */
    EPDU_SEC_RSA,        /** The RSA public key for this host could not be found. */
    EPDU_CIPHER_NOT_SUPPORTED, /** Not supported encryption cypher. */
    EPDU_RSA_E,          /** Problem while encrypting using the RSA algorithm. */
    EPDU_RSA_D,          /** Problem while decrypting using the RSA algorithm. */
    EPDU_ENCRYPT,        /** Generated when the encryption does not go throught. */
    EPDU_NADA,           /** The peer did not accept our PDU. */
} pdu_error_t;


struct pdu_str {
    struct link_str link;
    struct state_machine_str state;
    struct pdu_str *self;
    unsigned short flags;

    /** The error code is provided in here. */
    pdu_error_t errno;
    state_id_t  e_layer;
    /** */

    /** The marshaling layer uses these fields. */
    struct imago_control_block *icb_ptr;
    /** */

    /** The ADA rendezvous layer uses these feilds. */
    unsigned short genid;
    unsigned int   seqid;
    unsigned short ADAtype;
    timer_id timer;
    char retry;
    /** */



    /** The routing layer uses these fields. */
    char destination_server[MAX_DNS_NAME_SIZE];
    char source_server[MAX_DNS_NAME_SIZE];
    unsigned char ttl;
    /** */

    /** The connection layer uses these fields. */
    struct sockaddr_in remote_addr;
    struct sockaddr_in local_addr;
    void *c_id;
    long c_total;
    /** */

    /** The PDU is stored in a frame list. */
    struct link_head_str frames;
    /** */
};

void            pdu_free( struct pdu_str *);
struct pdu_str *pdu_new( );
struct pdu_str *pdu_clone( const struct pdu_str *, struct pdu_str * );
//struct pdu_str *pdu_forward( struct pdu_str * );
//struct pdu_str *pdu_backoff( struct pdu_str * );

void pdu_enqueue( struct pdu_str * );
struct pdu_str *pdu_dequeue( );

void pdu_set_state( struct pdu_str *, state_id_t );

int pdu_write_string( register struct pdu_str *, const char *, const int );
int pdu_write_text( register struct pdu_str *, const char * );
int pdu_write_int( register struct pdu_str *, int );
int pdu_write_short( register struct pdu_str *, short );
int pdu_write_byte( register struct pdu_str *, char );
int pdu_read_string( register struct pdu_str *, char *, const int );
int pdu_read_text( register struct pdu_str *, char *, int );
int pdu_read_int( register struct pdu_str *, int * );
int pdu_read_short( register struct pdu_str *, short * );
int pdu_read_byte( register struct pdu_str *, char * );

/** Network specific function. */
unsigned int pdu_size(register struct pdu_str *);
int pdu_send_frame( int sd, register struct pdu_str * );
int pdu_erase_frame( register struct pdu_str *, const int );

#endif
