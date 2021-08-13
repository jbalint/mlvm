/** $RCSfile: imago_layer_routing.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_routing.c,v 1.17 2003/03/21 19:39:02 gautran Exp $ 
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

/** ############################################################################ 
 *      Routing PDU Header. 
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                                                               |  | 
 * /                  Destination Server Name                      /  | 
 * |                                                               |  |   
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |Routing
 * |                                                               |  |protocol
 * /                     Source Server Name                        /  |header
 * |                                                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |      TTL      |                                               |  | 
 * +-+-+-+-+-+-+-+-+                                               | -+ 
 * /                           data                                / 
 * |                                                               | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 *
 ############################################################################ */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>


#include "include.h"
#include "imago_pdu.h"
#include "state_machine.h"
#include "imago_layer_connection.h"
#include "imago_layer_security.h"
#include "imago_layer_routing.h"

/** State machine ids. */
state_id_t imago_layer_routing_up_stateid = -1;
state_id_t imago_layer_routing_down_stateid = -1;
/** */

state_id_t imago_layer_routing_down( struct pdu_str * );
state_id_t imago_layer_routing_up( struct pdu_str * );


/** Our official name */
char hostname[MAX_DNS_NAME_SIZE] = {0x00};
/** Our name alias list. */
char **hostname_aliases = NULL;


/** Routing Layer Cache datastructure. */
#define CACHE_VALID 0x01
#define CACHE_STATIC (0x02 & CACHE_VALID)

struct cache_entry {
    char flags;  /** Specify if the entry is in use or free or static or ... */
    char hostname[MAX_DNS_NAME_SIZE]; /** Store the name of the looked up server. */
    struct in_addr hostaddr;  /** Store the ip address of the imadiate gateway. */
};
static struct cache_entry route_cache[MAX_ROUTING_CACHE_ENTRY];
#ifdef _REENTRANT
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
/** */

/** Initialization routine called once AND ONLY ONCE at the very begining. */
/** ! ! ! This is NOT a REENTRANT function ! ! ! */
void imago_layer_routing_init() {
    int i;
    struct hostent *localhost;
    /** Register the states in the state machine. */
    imago_layer_routing_up_stateid = state_machine_register( (state_action_t)imago_layer_routing_up );
    imago_layer_routing_down_stateid = state_machine_register( (state_action_t) imago_layer_routing_down );
    /** */
    /** We need to find/resolve the complete name of this machine. */
    if( hostname[0] == 0x00 )
	gethostname( hostname, sizeof(hostname) );
  
    /** Resolve ourselves. */
    if( (localhost = gethostbyname(hostname)) == NULL ) {
	warning( "Unable to resolve hostname properly: '%s'\n", hostname);
	return;
    }
    /** Now, Retreive the full name for this machine. */
    strncpy( hostname, localhost->h_name, sizeof(hostname)-1 ) ;
    /** Log  message. */
    notice("Full hostname resolved to: '%s'\n", hostname);
    /** Then, get the size required for the alias list. */
    for( i = 0; localhost->h_aliases[i]; i++ );
    /** Allocate the memory. */
    hostname_aliases = (char **)malloc((i+1) * sizeof(char *) );
    /** Clean it up. */
    memset( hostname_aliases, 0, (i+1) * sizeof(char *) );
    /** Copy all aliases. */
    for( i = 0; localhost->h_aliases[i]; i++ ) {
	hostname_aliases[i] = (char *)malloc(MAX_DNS_NAME_SIZE);
	strncpy( hostname_aliases[i], localhost->h_aliases[i], MAX_DNS_NAME_SIZE-1 ) ;
	/** Log  message. */
	info( "Host also known as: '%s'\n", hostname_aliases[i]);
    }
    /** clean up the cache table. */
    memset( route_cache, 0, sizeof(route_cache) );
}
void imago_layer_routing_cleanup() {
    /** Clean up this layer. */
    if( hostname_aliases ) 
	free( hostname_aliases );
    hostname_aliases = NULL;
}

/** */

int imago_layer_routing_getopt( int opt , void *arg ) {
    switch( opt ) {
    case R_SERVER_NAME:
	((int *)arg) = hostname;
	break;
    default:
	return -1;
    }
    return 0;
}
int imago_layer_routing_setopt( int opt, void *arg ) {
    switch( opt ) {
    case R_SERVER_NAME:
	if( arg == NULL ) 
	    memset( hostname, 0, sizeof(hostname) );
	else 	    
	    strncpy( hostname, arg, sizeof(hostname)-1 );
	hostname[sizeof(hostname)-1] = 0x00;
	break;
    default:
	return -1;
    }
    return 0;
}



/** Handy routine to return the current name of this machine. */
int get_imago_localhost( char *outbuf, const int olen ) {
    memset( outbuf, 0, olen );
    strncpy( outbuf, hostname, olen-1 ); 
    return strlen( outbuf );
}
/** Caching functions. */
static int find_in_cache( const char *name, struct in_addr *inet ) {
    /** Before starting a full lookup using a DNS request, we will first check
	our cache table to see it the IP address for this guy is known.
	This is the direct caching lookup... */
    int hv = strhash_np( name, MAX_ROUTING_CACHE_ENTRY );
#ifdef _REENTRANT
    pthread_mutex_lock( &cache_lock );
#endif 
    /** Lookup the table to see if we got something. */
    if( route_cache[hv].flags & CACHE_VALID ) {
	/** We got something in the cache... 
	    Check for collitions... */
	if( strncmp(name, route_cache[hv].hostname, MAX_DNS_NAME_SIZE) == 0 ) {
	    /** We got something in the cache. */
	    inet->s_addr = route_cache[hv].hostaddr.s_addr;
#ifdef _REENTRANT
	    pthread_mutex_unlock( &cache_lock );
#endif 
	    /** Return our finding. */
	    return 1;
	}
	/** Must be a collision...
	    Collision are not resolved and old entry will be replaced with the new one. */
    }
#ifdef _REENTRANT
    pthread_mutex_unlock( &cache_lock );
#endif 
    return 0;
}
static void update_the_cache( const char *name, const struct in_addr *inet ) {
    int hv = strhash_np( name, MAX_ROUTING_CACHE_ENTRY );
    /** Cache is updated only if the entry is dynamic. */
    if( !(route_cache[hv].flags & CACHE_STATIC) ) {
#ifdef _REENTRANT
	pthread_mutex_lock( &cache_lock );
#endif 
	/** Copy the host name. */
	strncpy( route_cache[hv].hostname, name, MAX_DNS_NAME_SIZE-1 );
	/** Store the IP. */
	route_cache[hv].hostaddr.s_addr = inet->s_addr;
	/** Change this entry status. */
	route_cache[hv].flags = CACHE_VALID;
#ifdef _REENTRANT
	pthread_mutex_unlock( &cache_lock );
#endif 
    }
}
/** */

/** Lookup a specific name. */
int lookup( const char *name, struct in_addr *inet ) {
    struct hostent *gw;
    int ret = -1;
#ifdef _REENTRANT
    static pthread_mutex_t libc_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
    if( find_in_cache(name, inet) > 0 ) {
	/** Found in cache. */
	return 1;
    }
    /** Not found in the cache... Start the DNS lookup. */
#ifdef _REENTRANT
    /** Some C library function are not REENTRANT. So make sure everything goes fine. */
    pthread_mutex_lock( &libc_lock );
#endif 
    /** We need to find/resolve the complete name of the destination machine. */
    gw = gethostbyname(name);
    /** Then extract the first IP address. */
    if( gw && gw->h_addr_list && gw->h_addr_list[0] ) {
	inet->s_addr = ((struct in_addr *)gw->h_addr_list[0])->s_addr;
	ret = 1;
    }
#ifdef _REENTRANT
    /** Some C library function are not REENTRANT. So make sure everything goes fine. */
    pthread_mutex_unlock( &libc_lock );
#endif 
    /** If we resolved this name, cache it in the routing cache table. */
    if( ret > 0 ) update_the_cache( name, inet );
    return ret;
}
/** Resolve the route to the host. */
/**
 * The routing algorithm used here is of a very high level.
 * The principle is such that the destination IP address may 
 * not be always known or lookup. In the other case, a direct connection 
 * is instanciated. But if the IP cannot be resolved, we have to find a way 
 * arround so the programmer does not have to explicitly make his/her imago
 * jump from one machine to another one. 
 * 
 * The idea is simple and is based upon name lookup and is better explained
 * with an example.
 * Looking at a machine named: 'gradhp-03.imago_lab.cis.uoguelph.ca'
 * From the outside world, its IP address is not resolvable. However, 
 * 'draco.cis.uoguelph.ca' knows about this host since draco is the 
 * primary gateway to the '.imago_lab.cis.uoguelph.ca' internal subnetwork
 * So to transmit this imago, we can just send it to draco and draco will 
 * take care of the forwarding for us. 
 * 
 * To make this idea more modular, we decide that draco will have a generic 
 * alias DNS name. In a similar way that most of the internet http server
 * are named 'www' our imago gateway will be named 'igw'. So draco will
 * be the same machine as 'igw.cis.uoguelph.ca' allowing imagoes from all 
 * over the world to penetrate the internal network using draco...
 * 
 */
int resolve_route( struct pdu_str *this_pdu ) {
    char name[MAX_DNS_NAME_SIZE];
    char *domain = this_pdu->destination_server;
    char *hostname = this_pdu->destination_server;
    /** At first, we will try to lookup the name in the cache. */
    if( find_in_cache(hostname, &this_pdu->remote_addr.sin_addr) > 0 ) {
	/** Nice hit ! :) */
	return 1;
    }
    /** Do not be alarm by this infinite loop ! 
	If the destination server is null terminated, 
	one of the many cases inside the loop will
	break out and terminate it... */
    while( 1 ) {
	/** Try to resolve this name directly into the pdu. */
	if( lookup( hostname, &this_pdu->remote_addr.sin_addr ) >= 0 ) {
	    /** Eureka ! ! ! We could resolve the IP. */
	    /** Let us cache this entry if we had to apply indirect routing. */
	    if( hostname != this_pdu->destination_server ) 
		update_the_cache( this_pdu->destination_server, &this_pdu->remote_addr.sin_addr );
	    return 1;
	} else {
	    /** We could not resolve that name. Try the network gateway. */
	    /** Gateway domain name is... */
	    if( (domain = strchr( domain+1, '.' )) == NULL ) {
		/** In the case we have exausted the complete path, 
		    the search is over. */
		return -1;
	    }
	    /** What name do we want to resolve ?? 
		The '.' is included in the domain. */
	    snprintf( name, MAX_DNS_NAME_SIZE-1, IMAGO_ROUTER_SYMB_NAME "%s", domain );
	    hostname = name;
	}
    }
    return -1;
}
/** */

/** Outgoing PDUs are handled here. */
/** ############################################################################ 
 *      Routing PDU Header. 
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                                                               |  | 
 * /                  Destination Server Name                      /  | 
 * |                                                               |  |   
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | Routing 
 * |                                                               |  |  protocol 
 * /                     Source Server Name                        /  |  header 
 * |                                                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |      TTL      |                                               |  | 
 * +-+-+-+-+-+-+-+-+                                               | -+ 
 * /                           data                                / 
 * |                                                               | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 *
 ############################################################################ */

/** This function resolve the MLVM Server Name of the PDU to an IP address. */
state_id_t imago_layer_routing_down( struct pdu_str *this_pdu ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 

    /** If this PDU is not for forwarding (we are not the source)... */
    if( !(this_pdu->flags & PDU_FLAG_FORWARDING) ) {
	/** Set the source server name in the PDU. */
	strcpy( this_pdu->source_server, hostname );
	/** Also, set the TTL to the MAX value (255). */
	this_pdu->ttl = 0xff;
    }
    /** Find where the domain name begins. */
    if( resolve_route( this_pdu ) < 0 ) {
	warning( "Routing layer was unable to resolve the forwarding server name \"%s\".\n", 
		 this_pdu->destination_server );
	this_pdu->errno = EPDU_HOST_NOT_FOUND;
	goto error;
    }
    /** We should have the gateway route. */
    /** Set the raw header for this PDU. */
    /** Last element is the TTL. */
    if( pdu_write_byte( this_pdu, this_pdu->ttl ) < 0 ) {
	this_pdu->errno = EPDU_MFRAME;
	goto error;
    }
    /** Then comes the source server name */
    if( pdu_write_text( this_pdu, this_pdu->source_server ) < 0 ) {
	this_pdu->errno = EPDU_MFRAME;
	goto error;
    }
    /** Then comes the destination server name */
    if( pdu_write_text( this_pdu, this_pdu->destination_server ) < 0 ) {
	this_pdu->errno = EPDU_MFRAME;
	goto error;
    }
    /** The header is complete. */
    /** Forward to the layer down. */
    return imago_layer_connection_down_stateid;

 error:
    /** In this case, if the PDU is ours, we send it back to the upper layers.
	Otherwise, we destroy it right away. */
#ifdef _ROUTER_ONLY
    pdu_free( this_pdu );
    return STATE_MACHINE_ID_INVALID;
#else
    if( (this_pdu->flags & PDU_FLAG_FORWARDING) || !(this_pdu->flags & PDU_TYPE_SITP) ) {
	/** Not from this machine... */
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;    
    } else {
	/** This is ours. */
	this_pdu->flags |= PDU_ERROR;
	this_pdu->e_layer = imago_layer_routing_down_stateid;
	return imago_layer_security_up_stateid;
    }
#endif
}

/** Incomming PDUs are handled here. */
/** ############################################################################ 
 *      Routing PDU Header. 
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |                                                               |  | 
 * /                  Destination Server Name                      /  | 
 * |                                                               |  |   
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |Routing
 * |                                                               |  |protocol
 * /                     Source Server Name                        /  |header
 * |                                                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |      TTL      |                                               |  | 
 * +-+-+-+-+-+-+-+-+                                               | -+ 
 * /                           data                                / 
 * |                                                               | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 *
 ############################################################################ */
/** The Destination Server Name is compare against outselves. If it match, 
    the PDU is for us, otherwise, it will forward the PDU to the outgoing side. */
state_id_t imago_layer_routing_up( struct pdu_str *this_pdu ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
    /** If the PDU is in error, just move it back up the stack. */
    if( this_pdu->flags & PDU_ERROR ) goto go_up;

    /** First, extract the header informations. */
    if( pdu_read_text(this_pdu, this_pdu->destination_server, sizeof(this_pdu->destination_server)) < 0 ) {
	/** Something is wrong with the server name. */
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    } 
    /** Extract the source server string. */
    if( pdu_read_text(this_pdu, this_pdu->source_server, sizeof(this_pdu->source_server)) < 0 ) {
	/** Something is wrong with the server name. */
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    } 
    /** Then, at last, get the TTL. */
    if( pdu_read_byte( this_pdu, &this_pdu->ttl ) < 0 ) {
	/** Something is wrong with the server name. */
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    } 
    /** */

    /** Compare the destination server address with any of our known names. */
    if( strncasecmp(hostname, this_pdu->destination_server, strlen(hostname)) ) {
	int i = 0;
	/** It does not seem to be for us... Keep checking. */
	for( i = 0; hostname_aliases[i]; i++ ) {
	    if( !strncasecmp(hostname_aliases[i], this_pdu->destination_server, strlen(hostname_aliases[i])) ) {
		/** This is for us. */
		break;
	    }
	}
	/** No match found ??? */
	if( !hostname_aliases[i] ) {
	    /** Get rid of the lost souls as soon as possible. */
	    if( !(this_pdu->ttl--) ) {
		pdu_free(this_pdu);
		return STATE_MACHINE_ID_INVALID;
	    }
	    /** Change the direction so the outgoing code will handle the PDU
		for forwarding. */
	    /** Set the FORWARDING flag. */
	    this_pdu->flags |= PDU_FLAG_FORWARDING;
	    /** Return this PDU. */
	    return imago_layer_routing_down_stateid;
	}
    }
    /** This packet is for us. */
 go_up:
#ifndef _ROUTER_ONLY
    /** Forward it to the upper layer. */
    return imago_layer_security_up_stateid;
#else
    pdu_free( this_pdu );
    return STATE_MACHINE_ID_INVALID;
#endif
}
/** */
