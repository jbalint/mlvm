/** $RCSfile: include.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: include.h,v 1.23 2003/03/24 14:07:48 gautran Exp $ 
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

#ifndef __IMAGO_INCLUDE_H_
#define __IMAGO_INCLUDE_H_

#ifndef __USE_GNU
# define __USE_GNU                           
#endif

#ifdef _REENTRANT
# include <pthread.h>
#endif

#ifdef MEMWATCH
#include "memwatch.h"
#endif

/** Defines how many PDU can be used at one time. */
#ifndef _IMAGO_MAX_PDU
# define IMAGO_MAX_PDU 1024
#else 
# define IMAGO_MAX_PDU _IMAGO_MAX_PDU
#endif

/** Where to store and lookup the known hosts. */
#ifndef _IMAGO_CONF_DIR
# define IMAGO_CONF_DIR "/etc/mlvm"
#else
# define IMAGO_CONF_DIR _IMAGO_CONF_DIR
#endif

/** Where to store and lookup the known hosts. */
#ifndef _IMAGO_KNOWN_HOST_FILE
# define IMAGO_KNOWN_HOST_FILE IMAGO_CONF_DIR "/known_hosts"
#else
# define IMAGO_KNOWN_HOST_FILE _IMAGO_KNOWN_HOST_FILE
#endif

/** PDU backup defines. */
/** Defines where to backup the PDUs. */
#ifndef _IMAGO_PDU_SPOOL
# define IMAGO_PDU_SPOOL "/var/spool/mlvm/transport"
#else 
# define IMAGO_PDU_SPOOL _IMAGO_PDU_SPOOL
#endif

/** What string will be appended at the begining of the backup file name. */
#ifndef _IMAGO_PDU_SPOOL_PRE
# define IMAGO_PDU_SPOOL_PRE ""
#else 
# define IMAGO_PDU_SPOOL_PRE _IMAGO_PDU_SPOOL_PRE
#endif

/** What string will be appended at the begining of the backup file name. */
#ifndef _IMAGO_PDU_SPOOL_SEP
# define IMAGO_PDU_SPOOL_SEP  '#' 
#else 
# define IMAGO_PDU_SPOOL_SEP _IMAGO_PDU_SPOOL_SEP
#endif

/** What extention will be appended at the end of the backup file name. */
#ifndef _IMAGO_PDU_SPOOL_EXT
# define IMAGO_PDU_SPOOL_EXT ".pdu"
#else 
# define IMAGO_PDU_SPOOL_EXT _IMAGO_PDU_SPOOL_EXT
#endif
/** */

/** Defines how many concurrent connection can be used at one time. */
#ifndef _IMAGO_MAX_CONNECTION
# define IMAGO_MAX_CONNECTION 255
#else 
# define IMAGO_MAX_CONNECTION _IMAGO_MAX_CONNECTION
#endif


/** The DNS Hostname for the host is bounded to a maximum size of 64 bytes. */
//#ifndef _MAX_DNS_NAME_SIZE
//# define MAX_DNS_NAME_SIZE 64
//#else 
//# define MAX_DNS_NAME_SIZE _MAX_DNS_NAME_SIZE
//#endif

/** By default, the imago server listen to this specific port. */
#ifndef _IMAGO_SERVER_PORT
# define IMAGO_SERVER_PORT 2223
#else 
# define IMAGO_SERVER_PORT _IMAGO_SERVER_PORT
#endif

/** If the imago_router is compiled as a multithreaded application, it
    will have a maximum number of thread seating there to handle incomming PDUs. */
#if defined (_REENTRANT)
// && (_ROUTER_ONLY)
# ifndef _IMAGO_PROTOCOL_MAX_THREAD
#  define IMAGO_PROTOCOL_MAX_THREAD 5
# else
#  define IMAGO_PROTOCOL_MAX_THREAD _IMAGO_PROTOCOL_MAX_THREAD
# endif
#endif 

/** What is the generic imago gateway symbolic name. */
#ifndef _IMAGO_ROUTER_SYMB_NAME
#define IMAGO_ROUTER_SYMB_NAME "igw"
#else
#define IMAGO_ROUTER_SYMB_NAME _IMAGO_ROUTER_SYMB_NAME
#endif

/** The error logger function. */
#include <syslog.h>
#include "log.h"
/** */

/** State machine dependant defines. */
#ifndef _STATE_MACHINE_INCR
# define STATE_MACHINE_INCR 20
#else
# define STATE_MACHINE_INCR _STATE_MACHINE_INCR
#endif
/** */


/** The size of the cache table for the routing layer. */
#ifndef _MAX_ROUTING_CACHE_ENTRY
# define MAX_ROUTING_CACHE_ENTRY 256
#else
# define MAX_ROUTING_CACHE_ENTRY _MAX_ROUTING_CACHE_ENTRY
#endif
/** */



/** Security feature enabled */
#ifndef _IMAGO_SECURITY_ENABLED
# undef IMAGO_SECURITY_ENABLED 
#else
# define IMAGO_SECURITY_ENABLED 
# include <openssl/opensslconf.h>
# include <openssl/opensslv.h>
# ifdef NO_DES
#  define NO_3DES
# endif
#endif
/** */


#ifdef IMAGO_SECURITY_ENABLED
/** Supported encryption functions. */
//#define NO_IDEA
//#define NO_DES
//#define NO_3DES
//#define NO_RC4
//#define NO_BF

/** Some people want there system ultra secure. */
//#define HIGH_SECURITY

/** Where to store and load our private RSA key. */
# ifndef _IMAGO_RSA_PRIV_FILE
#  define IMAGO_RSA_PRIV_FILE IMAGO_CONF_DIR "/imago_host_key"
# else
#  define IMAGO_RSA_PRIV_FILE _IMAGO_RSA_PRIV_FILE
# endif

/** The RSA host key must have a specific size. */
# ifndef _IMAGO_RSA_KEY_SIZE
#  define IMAGO_RSA_KEY_SIZE 2048
# else
#  define IMAGO_RSA_KEY_SIZE _IMAGO_RSA_KEY_SIZE
# endif

/** The DSA host key must have a specific size. */
# ifndef _IMAGO_DSA_KEY_SIZE
#  define IMAGO_DSA_KEY_SIZE 512
# else
#  define IMAGO_DSA_KEY_SIZE _IMAGO_DSA_KEY_SIZE
# endif
#endif
/** */

/** Signature feature enable */
#ifdef IMAGO_SECURITY_ENABLED
# ifndef _IMAGO_SIGN_ENABLED
#  undef IMAGO_SIGN_ENABLED
# else
#  define IMAGO_SIGN_ENABLED
# endif
#else
# ifdef _IMAGO_SIGN_ENABLED
 #  warning "Security feature is disabled, Signature must be disabled as well."
# endif
# undef IMAGO_SIGN_ENABLED
#endif
/** */


#include "version.h"

#include "imago_icb.h"
#include "transport.h"

/** Terminate the Server application. */
extern int imago_server_stop;

#define LOG_ERROR LOG_ERR

#endif
