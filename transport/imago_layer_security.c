/** $RCSfile: imago_layer_security.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: imago_layer_security.c,v 1.18 2003/03/24 14:07:48 gautran Exp $ 
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
 *   Security headers
 *
 * Security disabled header (SITP & SICP) 
 *  
 *  0
 *  0 1 2 3 4 5 6 7  
 * +-+-+-+-+-+-+-+-+ -+ 
 * |0|  Reserved   |  | Security disabled header 
 * +-+-+-+-+-+-+-+-+ -+ 
 * 
 * 
 *  RSA Public key exchange SICP PDU header
 *  
 *  
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   
 * |1|   Flags     |  Cypher Mask  |            Request ID         |   
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |          Modulus len          |                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+      Public Modulus           /  |RSA 
 * |                                                               |  |Public 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |Host 
 * |          Exponent len         |                               |  |Key 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+      Public Exponent          /  | 
 * |                                                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 * |       Prime Number len        |                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+         Prime Number          /  | 
 * |                                                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |      160-bit subprime len     |                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+      160-bit subprime         /  |DSA 
 * |                                                               |  |Public 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  |Host 
 * |   Generator of subgroup len   |                               |  |Key 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+    Generator of subgroup      /  | 
 * |                                                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  | 
 * |         Public key len        |                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+          Public key           /  | 
 * |                                                               |  | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -+ 
 *  
 * 
 * 
 *  SITP PDU header. 
 *  
 *  
 *  0                   1                   2                   3 
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  -+ 
 * |1|  Reserved   |     Cypher    |   Encrypted Session key len   |   |  
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   | 
 * |                                                               |   | 
 * /                      Encrypted Session key                    /   |Security 
 * |                                                               |   |Header 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+   | 
 * |    Encrypted Signature len    |                               |   | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  Encrypted Signature          /   | 
 * |                                                               |   | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  -+ 
 * |                                                               | 
 * /                            data                               / 
 * |                                                               | 
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 *  
 * 
 * ########################################################################## */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/** Must be first for its #define. */
#include "include.h"

#ifdef IMAGO_SECURITY_ENABLED
# include <openssl/rsa.h>
# include <openssl/dsa.h>
# include <openssl/sha.h>
# include <openssl/evp.h>
# include <openssl/rand.h>
# include <openssl/err.h>
/** openssl/err.h has a 
    #define errno blablabla 
    Get rid of it.
*/
# undef errno
#endif /** #ifdef IMAGO_SECURITY_ENABLED */

#include "imago_pdu.h"
#include "pdu_backup.h"
#include "state_machine.h"
#include "imago_layer_ADArendezvous.h"
#include "imago_layer_routing.h"
#include "imago_layer_security.h"
#ifdef IMAGO_SECURITY_ENABLED
#include "cipher.h"
#include "frame.h"

/** Sensitive data. This is the unique server host key. */
static Key *host_key = NULL;
# ifdef _REENTRANT
pthread_mutex_t host_key_lock = PTHREAD_MUTEX_INITIALIZER;
# endif
/** */

/** Sequence ID used for requesting the RSA keys. */
static unsigned int sequence = 0;
# ifdef _REENTRANT
pthread_mutex_t seq_lock = PTHREAD_MUTEX_INITIALIZER;
# endif
#endif /** #ifdef IMAGO_SECURITY_ENABLED */


/** State machine ids. */
state_id_t imago_layer_security_up_stateid = -1;
state_id_t imago_layer_security_down_stateid = -1;
/** */

state_id_t imago_layer_security_up( struct pdu_str * );
state_id_t imago_layer_security_down( struct pdu_str * );

void rsa_cb(int t,int n,void *cb_arg) {
    notice( "." );
}
void dsa_cb(int t,int n,void *cb_arg) {
    notice( "+" );
}
static const char rnd_seed[] = "string to make the random number generator think it has entropy";

/** Initialization routine called once at the very first begining. */
void imago_layer_security_init() {
#ifdef IMAGO_SECURITY_ENABLED
    int changed = 0;
#endif

    /** Register the states in the state machine. */
    imago_layer_security_up_stateid = state_machine_register( (state_action_t)imago_layer_security_up );
    imago_layer_security_down_stateid = state_machine_register( (state_action_t) imago_layer_security_down );
    /** */
    /** Seed the rendom number generator. */
    srandom(time(NULL));

#ifdef IMAGO_SECURITY_ENABLED
    /** Initialize the encryption routines. */
    SSLeay_add_all_algorithms();
    ERR_load_crypto_strings();
    RAND_seed(rnd_seed, sizeof(rnd_seed) );
    /** */

    host_key = Key_new();
    notice( "Loading RSA host key.\n");
    /** Load the RSA private host key from the file. */
    load_private_key( host_key, IMAGO_RSA_PRIV_FILE, "MLVM Imago" );

    if( host_key->rsa == NULL ) {
	/** No RSA key have been defined. */
	notice( "No RSA key found.\nGenerating RSA host key." );
	/** Generate one. */
	host_key->rsa = RSA_generate_key( IMAGO_RSA_KEY_SIZE, 35, rsa_cb, NULL );
	notice( "done.\n" );
	changed = 1;
    }
    if( host_key->rsa == NULL ) {
	error( "Unable to load our host RSA key.\n" );
	exit(1);
    }

# ifdef IMAGO_SIGN_ENABLED
    if( host_key->dsa == NULL ) {
	/** No DSA key have been defined. */
	/** Generate one. */
	host_key->dsa = DSA_generate_parameters(IMAGO_DSA_KEY_SIZE, NULL, 0, NULL, NULL, dsa_cb, NULL);
	notice( "done.\n" );
	if( host_key->dsa == NULL ) {
	    error( "Unable to generate the DSA parameters.\n" );
	    exit(1);
	}
	if( DSA_generate_key( host_key->dsa ) != 1 ) {
	    error( "Unable to Generate the DSA key.\n" );
	    exit(1);
	}
	changed = 1;
    }
    if( host_key->dsa == NULL ) {
	error( "Unable to load our host RSA key.\n" );
	exit(1);
    }
# endif
    /** If the key have been generated, save them to the file. */
    if( changed ) {
	error( "Host key changed. \nSaving host key to file.\n" );
	if( save_private_key( host_key, IMAGO_RSA_PRIV_FILE, "MLVM Imago" ) < 0 ) {
	    error( "Unable to write our private host key.\n" );
	    exit(1);
	}
    }
    /** Initialize all suported encryption algorithms. */
    host_key->cipher_mask = SEC_CIPHER_NONE;
# ifndef NO_IDEA
    host_key->cipher_mask |= SEC_CIPHER_IDEA;
    info( "Loading IDEA encyption algorithm...\n" );
# endif
# ifndef NO_DES
    host_key->cipher_mask |= SEC_CIPHER_DES;
    info( "Loading DES encyption algorithm...\n" );
# endif
# ifndef NO_3DES
    host_key->cipher_mask |= SEC_CIPHER_3DES;
    info( "Loading 3DES encyption algorithm...\n" );
# endif
# ifndef NO_RC4
    host_key->cipher_mask |= SEC_CIPHER_BROKEN_RC4;
    info( "Loading RC4 encyption algorithm...\n" );
# endif
# ifndef NO_BF
    host_key->cipher_mask |= SEC_CIPHER_BLOWFISH;
    info( "Loading BLOWFISH encyption algorithm...\n" );
# endif
#endif /** #ifdef IMAGO_SECURITY_ENABLED */
}
/** */

int imago_layer_security_getopt( int opt , void *arg ) {
    return -1;
}
int imago_layer_security_setopt( int opt, void *arg ) {
    return -1;
}


#ifdef IMAGO_SECURITY_ENABLED
static unsigned int get_seq_id() {
    unsigned int seq;
# ifdef _REENTRANT
    pthread_mutex_lock( &seq_lock ); 
# endif
    seq = sequence++;
# ifdef _REENTRANT
    pthread_mutex_unlock( &seq_lock ); 
# endif
    return seq;
}


/**
 * Encrypt the content of the PDU frames using the 32 bytes session key provided.
 * The encryption is executed using the specified cipher. If the particular cipher is
 * not is supported an error is returned. 
 */
int symetric_encrypt( struct pdu_str *this_pdu, unsigned char *session_key, unsigned int *ciphers ) {
    int ret = do_crypto( this_pdu, session_key, SEC_SESSION_KEY_LENGTH, ciphers, CIPHER_ENCRYPT );
    if( ret < 0 ) 
	this_pdu->errno = EPDU_ENCRYPT;
    return ret;
}

/**
 * Decrypt the content of the PDU frames using the 32 bytes session key provided.
 * The encryption is executed using the specified cipher. If the particular cipher is
 * not is supported an error is returned. 
 */
int symetric_decrypt( struct pdu_str *this_pdu, unsigned char *session_key, unsigned int cipher ) {
    return do_crypto( this_pdu, session_key, SEC_SESSION_KEY_LENGTH, &cipher, CIPHER_DECRYPT );
}

int rsa_public_encrypt( unsigned char *outbuf, unsigned char *inbuf, int len, RSA *key ) {
    int ilen = len;
    if (BN_num_bits(key->e) < 2 || !BN_is_odd(key->e)) {
	error( "rsa_public_encrypt() exponent too small or not odd\n" );
	return -1;
    }
    if ((len = RSA_public_encrypt(ilen, inbuf, outbuf, key, RSA_PKCS1_PADDING)) <= 0) {
	error( "rsa_public_encrypt() failed\n" );
	return -1;
    }
    return len;
}

int rsa_private_decrypt( unsigned char *outbuf, unsigned char *inbuf, int len, RSA *key ) {
    int ilen = len;
    if ((len = RSA_private_decrypt(ilen, inbuf, outbuf, key, RSA_PKCS1_PADDING)) <= 0) {
	debug( "rsa_private_decrypt() failed\n" );
	return -1;
    }
    return len;
}

/** ############################################################################
 *
 *   Send our own RSA public key
 *
 * ########################################################################## */
int rsa_host_key_send( const char *dest, unsigned short reqid, int flags ) {
    struct pdu_str *control_pdu;
    debug( "%s(%s, %d, %X)\n", __PRETTY_FUNCTION__, dest, reqid, flags ); 
    /** Create the control PDU to send the Accept? packet. */
    if( (control_pdu = pdu_new()) == NULL ) {
	return -1;
    }
    /** Initialize it all. */
    /** Mark it as an SICP. */
    control_pdu->flags &= ~PDU_TYPE_SITP;
    /** Write the destination server to the PDU. */
    strcpy( control_pdu->destination_server, dest );
    /** Set the state for this new pdu. */
    pdu_set_state( control_pdu, imago_layer_routing_down_stateid );
    /** Write down the Security header. */
# ifdef IMAGO_SIGN_ENABLED
    /** Store the public components of our DSA key. */
#  ifdef _REENTRANT
    pthread_mutex_lock( &host_key_lock );
#  endif
    /** Store the public key y = g^x. */
    if( put_bignum( control_pdu, host_key->dsa->pub_key ) < 0 )
	goto error;
#  ifdef _REENTRANT
    pthread_mutex_unlock( &host_key_lock );
#  endif
#  ifdef _REENTRANT
    pthread_mutex_lock( &host_key_lock );
#  endif
    /** Store the generator of subgroup (public). */
    if( put_bignum( control_pdu, host_key->dsa->g ) < 0 )
	goto error;
#  ifdef _REENTRANT
    pthread_mutex_unlock( &host_key_lock );
#  endif
#  ifdef _REENTRANT
    pthread_mutex_lock( &host_key_lock );
#  endif
    /** Store the 160-bit subprime, q | p-1 (public). */
    if( put_bignum( control_pdu, host_key->dsa->q ) < 0 )
	goto error;
#  ifdef _REENTRANT
    pthread_mutex_unlock( &host_key_lock );
#  endif
#  ifdef _REENTRANT
    pthread_mutex_lock( &host_key_lock );
#  endif
    /** Store the prime number (public). */
    if( put_bignum( control_pdu, host_key->dsa->p ) < 0 )
	goto error;
#  ifdef _REENTRANT
    pthread_mutex_unlock( &host_key_lock );
#  endif
    /** Indicate the DSA key is in the packet. */
    flags |= SEC_TYPE_DSA_KEY;
# endif /** # ifdef IMAGO_SIGN_ENABLED */
    /** Store the public components of our RSA key. */  
# ifdef _REENTRANT
    pthread_mutex_lock( &host_key_lock );
# endif
    /** Store the public exponent of the host key. */
    if( put_bignum( control_pdu, host_key->rsa->e ) < 0 )
	goto error;
# ifdef _REENTRANT
    pthread_mutex_unlock( &host_key_lock );
    /** Just to give a chance to others to use the key.
	Since puting a big number may be time consuming... */
    /** Let us been picky ! ! ! */ 
    pthread_mutex_lock( &host_key_lock );
# endif
    /** Store the public modulus of the host key. */
    if( put_bignum( control_pdu, host_key->rsa->n ) < 0 ) 
	goto error;
# ifdef _REENTRANT
    pthread_mutex_unlock( &host_key_lock );
# endif

    /** Last is the Request ID. */
    if( pdu_write_short( control_pdu, reqid & 0xffff) < 0 )
	goto error;
# ifdef _REENTRANT
    pthread_mutex_lock( &host_key_lock );
# endif

    /** Mark down what Cipher we do support. */
    if( pdu_write_byte( control_pdu, host_key->cipher_mask & 0xff ) < 0 )
	goto error;
# ifdef _REENTRANT
    pthread_mutex_unlock( &host_key_lock );
# endif

    /** Then comes the flags. */
    if( pdu_write_byte( control_pdu, SEC_TYPE_ENABLED | (flags & 0xff) | SEC_TYPE_RSA_KEY ) < 0 )
	goto error;
    /** */

    /** Once this is all set, push the pdu into the IO queue to be sent. */
    pdu_enqueue( control_pdu );
    /** Return a success. */
    return 0;

 error:
    /** Destroy the pdu. */
    pdu_free( control_pdu );
    /** Return an error. */
    return -1;
}

/** ############################################################################
 *
 * When a TRANSPORT Packet has to be sent:
 *
 * 1) Do we have the destination RSA key ? 
 *    'Yes' - goto step 2 
 *    'No'  - 1a) Send SICP RSA key exchange request with own public key. 
 *            1b) Receive SICP RSA key exchange response with remote public key.
 *  
 * 2) Generate a random session key. 
 *  
 * 3) Encrypt data with session key. 
 *     
 * 4) Encrypt session key with destination's RSA public key. 
 *  
 * 4a) Sign session key using own RSA private key (optional). 
 * 
 * 5) Send encrypted session key, signature and encrypted data. 
 * 
 * ########################################################################## */
state_id_t imago_layer_security_transport_down( struct pdu_str *this_pdu ) {
    Key *dest_key;
    char flags = 0;
    char backup_key[BACKUP_NAME_MAX]; /** Should be enough. */
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
    dest_key = Key_new();
    /** 1) First, load the public RSA key for the given host. */
    if( load_key( this_pdu->destination_server, dest_key ) < 0 ) {
	/** 1) 'No' *//** No public RSA key could be found. */
	unsigned int seqid = get_seq_id();
	/** Request it. */
	if( rsa_host_key_send( this_pdu->destination_server, seqid & 0xffff, SEC_TYPE_REQUEST ) ) {
	    /** We where unable to generate an RSA key exchange PDU request. 
		Notify the upper layer. */
	    this_pdu->errno = EPDU_SEC_RSA;
	    goto error;
	}
	/** We send our request, now we just have to wait for the response... */
	/** Meanwhile, backup the PDU. */
	/** Create a backup ID using the sequence ID. */
	snprintf( backup_key, sizeof(backup_key), SEC_PREFIX "%d", seqid );
	/** Backup this PDU. */
	if( backup_save(backup_key, this_pdu) < 0 ) {
	    this_pdu->errno = EPDU_NBACK;
	    goto error;
	}
	/** Then, we can safely destroy this PDU while waiting for the host key to come back... */
	Key_free( dest_key );
	pdu_free( this_pdu );
	/** Return nothing to do. */
	return STATE_MACHINE_ID_INVALID;
    }
    /** 1) 'Yes' */
    /** We have loaded the destination server RSA key. */
    {
	unsigned int rand;
	unsigned char session_key[SEC_SESSION_KEY_LENGTH];
	unsigned int cipher;
	int i;
# ifdef _REENTRANT
	pthread_mutex_lock( &host_key_lock );
# endif
	cipher = dest_key->cipher_mask & host_key->cipher_mask;
# ifdef _REENTRANT
	pthread_mutex_unlock( &host_key_lock );
# endif

	/** 2) Generate a random session key. */
	/**
	 * Generate an encryption key for the session. The key is a 256 bit
	 * random number, interpreted as a 32-byte key, with the least
	 * significant 8 bits being the first byte of the key.
	 */
	for (i = 0; i < SEC_SESSION_KEY_LENGTH; i++) {
	    if (i % 4 == 0)
		rand = random();
	    session_key[i] = rand & 0xff;
	    rand >>= 8;
	}
	/** 3) Encrypt data with session_key. */
	if( symetric_encrypt( this_pdu, session_key, &cipher ) < 0 ) 
	    /** We could not encrypt the PDU. */
	    goto error;

	/** 4) Encrypt session key with destination's RSA public key. */
	{
	    unsigned char e_session_key[RSA_size(dest_key->rsa)];
	    int e_len = rsa_public_encrypt( e_session_key, session_key, SEC_SESSION_KEY_LENGTH,  dest_key->rsa );

	    /** Make sure the RSA encryption worked. */
	    if( e_len < 0 ) {
		this_pdu->errno = EPDU_RSA_E;
		goto error;
	    }
	    /** 4a) Sign session key using own DSA private key (optional). */
# ifdef IMAGO_SIGN_ENABLED
	    if( dest_key->dsa ) {
		unsigned char sign[DSA_size(dest_key->dsa)];
		int siglen;
		unsigned char digest[SHA_DIGEST_LENGTH];
		/** Get the SHA digest of the session key. */
		SHA1( session_key, SEC_SESSION_KEY_LENGTH, digest );
		/** Sign the digest. */
		if( DSA_sign(0, digest, sizeof(digest), sign, &siglen, host_key->dsa) == 1 ) {
		    /** The signature was successful, encrypt it with the public key. */
		    unsigned char e_sign[RSA_size(dest_key->rsa)];
		    int e_siglen = rsa_public_encrypt( e_sign, sign, siglen,  dest_key->rsa );
		    if( e_siglen > 0 ) {
			/** We have our encrypted signature. 
			    Store it in the PDU and flag it. */
			/** Write the encrypted signature. */
			if( pdu_write_string( this_pdu, e_sign, e_siglen ) < 0 ) {
			    this_pdu->errno = EPDU_MFRAME;
			    goto error;
			}
	     
			/** Write the encrypted signature len. */
			if( pdu_write_short( this_pdu, (short)e_siglen ) < 0 ) {
			    this_pdu->errno = EPDU_MFRAME;
			    goto error;
			}
			flags |= SEC_TYPE_SIGNED;
		    }
		}
	    }
# endif
	    /** 5) Send encrypted session key, signature and encrypted data. */
	    /** Write the security header. */

	    /** Write the encrypted session key. */
	    if( pdu_write_string( this_pdu, e_session_key, e_len ) < 0 ) {
		this_pdu->errno = EPDU_MFRAME;
		goto error;
	    }

	    /** Write the encrypted session key len. */
	    if( pdu_write_short( this_pdu, (short)e_len ) < 0 ) {
		this_pdu->errno = EPDU_MFRAME;
		goto error;
	    }

	    /** Write which cipher has been used to encrypt the data. */
	    if( pdu_write_byte( this_pdu, cipher ) < 0 ) {
		this_pdu->errno = EPDU_MFRAME;
		goto error;
	    }

	    /** Write some flags to indicate if the signature is present. */
	    if( pdu_write_byte( this_pdu, SEC_TYPE_ENABLED | flags ) < 0 ) {
		this_pdu->errno = EPDU_MFRAME;
		goto error;
	    }
	}
    } 
    Key_free( dest_key );
    /** The PDU is complete. */
    return imago_layer_routing_down_stateid;

 error:
    Key_free( dest_key );
    /** Something went wrong. Abort and forward back the ICB to where it belongs. */
    this_pdu->flags |= PDU_ERROR;
    this_pdu->e_layer = imago_layer_security_down_stateid;
    /** And return an invalid state. */
    return imago_layer_ADArendezvous_up_stateid;
}

state_id_t imago_layer_security_control_down( struct pdu_str *this_pdu ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
    /** This comes from the upper layer and do not conserns us. 
	Write a one byte header indicating the no Security information is present. */
    if( pdu_write_byte( this_pdu, 0 ) < 0 ) {
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    }
    return imago_layer_routing_down_stateid;
}
#endif /** #ifdef IMAGO_SECURITY_ENABLED */

/** ############################################################################
 *
 * When a CONTROL Packet is received:
 *
 *   Store the source RSA public key 
 *
 *   Send our own RSA public key
 *
 * ########################################################################## */
state_id_t imago_layer_security_control_up( struct pdu_str *this_pdu ) {
    unsigned char flags = 0;
    struct pdu_str *inc_pdu = NULL;
    char backup_key[BACKUP_NAME_MAX];
#ifdef IMAGO_SECURITY_ENABLED
    short         seqid = 0;
    Key           *key = NULL;
#endif
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
    /** First of all, retreive the security header. */
    /** Get the flags. */
    if( pdu_read_byte( this_pdu, &flags ) < 0 ) {
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    }
    /** If this PDU is for us, process it. */
    if( ! (flags & SEC_TYPE_ENABLED) ) {
	/** Not for us, forward to the upper layer. */
	return imago_layer_ADArendezvous_up_stateid;    
    }
#ifdef IMAGO_SECURITY_ENABLED
    key = Key_new();

    /** Load the cipher mask. */
    pdu_read_byte( this_pdu, &key->cipher_mask );

    /** Load the sequence ID. */
    pdu_read_short( this_pdu, &seqid );

    /** Then, load the RSA key if provided. */
    if( flags & SEC_TYPE_RSA_KEY ) {
	/** Load and check the RSA key provided to make sure this is the real guy. */
	if( (key->rsa = RSA_new()) == NULL ) {
	    error( "Unable to create a new RSA key.\n" );
	    goto error;
	}
	/** Load the public modulus of the key. */
	if( get_bignum( this_pdu, &key->rsa->n ) < 0 ) {
	    warning( "Bad RSA key (bad modulus).\n" );
	    goto error;
	}
	/** Load the public exponent of the host key. */
	if( get_bignum( this_pdu, &key->rsa->e ) < 0 ) {
	    warning( "Bad RSA key. (bad exponent)\n" );
	    goto error;
	}
    }
    /** Then, load the DSA key if provided. */
    if( flags & SEC_TYPE_DSA_KEY ) {
	if( (key->dsa = DSA_new()) == NULL ) {
	    error( "Unable to create a new DSA key.\n" );
	    goto error;
	}
	/** Read the public components of our DSA key. */
	/** Read the prime number (public). */
	if( get_bignum( this_pdu, &key->dsa->p ) < 0 ) {
	    warning( "Bad DSA key. (prime number)\n" );
	    goto error;
	}
	/** Read the 160-bit subprime, q | p-1 (public). */
	if( get_bignum( this_pdu, &key->dsa->q ) < 0 ) {
	    warning( "Bad DSA key. (160-bit subprime)\n" );
	    goto error;
	}
	/** Read the generator of subgroup (public). */
	if( get_bignum( this_pdu, &key->dsa->g ) < 0 ) {
	    warning( "Bad DSA key. (generator of subgroup)\n" );
	    goto error;
	}
	/** Read the public key y = g^x. */
	if( get_bignum( this_pdu, &key->dsa->pub_key ) < 0 ) {
	    warning( "Bad DSA key. (pub_key)\n" );
	    goto error;
	}
    }
#ifdef HIGH_SECURITY
    if( key->dsa == NULL || key->rsa == NULL ) 
	/** We cannot accept not to have both. */
	goto error;
#endif
    /** If this was the response to a request, that has been sent
	a while ago, then we have to reload the backup to make sure the
	source_server name match the name we use to send the request.
	The problem happens when the user uses a different name than the
	official one, to contact the server. For example, in the imago_lab
	network, the server 'draco.imago_lab.cis.uoguelph.ca' can be reached
	using the name 'draco' from within the network. In such case, the security
	layer would get crazy. The kex response would bear the full name, while 
	the request would only bear the short name. When releasing the 
	PDU below, it would generate another kex round because the key db
	only knows about 'draco.imago_lab.cis.uoguelph.ca' but the PDU 
	requests for 'draco'... The cyrcle would never ever end... */
    if( !(flags & SEC_TYPE_REQUEST) ) {
	/** If this is not a request, but a response, we may have initiated the request.
	    In that case, it means that we have a PDU waiting for us in the backup directory...
	    Load it an pass it to ourselves again. */
	snprintf( backup_key, sizeof(backup_key), SEC_PREFIX "%d", seqid );
	if( (inc_pdu = backup_load(backup_key, NULL)) != NULL ) {
	    /** It may happen that the host who answered us does not bear the same name as the host
		the we have been requesting the information from. */
	    /** Yes, it was there... Add it to the queue. */
	    pdu_set_state( inc_pdu, imago_layer_security_down_stateid );
	    /** Then we have to check if the names matches. 
		Actually, the strcmp would have to scan all characters
		in most cases (if the names are identicals).
		So, we just save the extra step for the copy by doing 
		the copy right away... If the names are identicals,
		well... It would not be more work... If they are different,
		We save the compare step... (Candle tip savings...)		
	    */
	    strncpy( this_pdu->source_server, inc_pdu->destination_server, MAX_DNS_NAME_SIZE-1 );
	} else {
	    warning( 
		    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		    "@@      ! ! ! !  SECURITY WARNING  ! ! ! !      @@\n"
		    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		    "   Received response to kex, but no requests where sent???\n"
		    "   Server: '%s'\n"
		    "   Request ignored...\n",
		    this_pdu->source_server );
	    goto error;
	}
    }
    /** If the host is not known, the key will be saved authomaticaly. */
    if( check_save_key( this_pdu->source_server, key ) < 0 ) {
	warning( 
		"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		"@@      ! ! ! !  SECURITY WARNING  ! ! ! !      @@\n"
		"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		"   Unchecked public host key for server:          \n"
		"   '%s'\n"
		"   Request ignored...\n",
		this_pdu->source_server );
	goto error;    
    }
    /** If the server has been found, then check if it was a request or a response. */
    if( flags & SEC_TYPE_REQUEST ) {
	/** Then send a response. */
	rsa_host_key_send( this_pdu->source_server, seqid, 0 );
    } else if( inc_pdu ) {
	pdu_enqueue( inc_pdu );
	/** Destroy the backup. */
	backup_cancel( backup_key );
	/** Clear the pointer so the PDU does not get destroy. */
	inc_pdu = NULL;
    }
    /** Clean all allocated resources. */
 error:
    if( inc_pdu ) pdu_free( inc_pdu );
    Key_free(key);

#else /** #ifdef IMAGO_SECURITY_ENABLED */
    /** The server does not suport security but the PDU is encrypted. */
    warning( "Encryption disable but PDU is encrypted...\n" );
#endif /** #ifdef IMAGO_SECURITY_ENABLED */

    pdu_free( this_pdu );
    return STATE_MACHINE_ID_INVALID;
}

/** ############################################################################
 *
 * When a TRANSPORT Packet is received:
 *
 * 1) Receive SITP PDU, extract encrypted session key and decrypt it. 
 *
 * 2) Verify signature (optional). 
 * 
 * 3) Decrypt data using session key.
 *
 * ########################################################################## */
state_id_t imago_layer_security_transport_up( struct pdu_str *this_pdu ) {
    unsigned char flags = 0;
#ifdef IMAGO_SECURITY_ENABLED
    unsigned char cipher = 0;
    short e_session_key_len;
    short e_siglen;
#endif 
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu );
    if( this_pdu->flags & PDU_ERROR )
	return imago_layer_ADArendezvous_up_stateid; 

    /** Get the flags. */
    if( pdu_read_byte( this_pdu, &flags ) < 0 ) {
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
    }
    /** If this PDU is for us, process it. */
    if( !(flags & SEC_TYPE_ENABLED) ) {
#ifdef HIGH_SECURITY
	warning( 
		"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		"@        ! ! ! ! WARNING: SECURITY FAILURE ! ! ! !        @\n"
		"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		" The server sent us a no-encrypted PDU but we do not allow such thing.\n"
		" Host: '%s'\n"
		" Request ignored.\n",
		this_pdu->source_server );
	pdu_free( this_pdu );
	return STATE_MACHINE_ID_INVALID;
#else
	/** Not for us, forward to the upper layer. */
	return imago_layer_ADArendezvous_up_stateid;    
#endif
    }
#ifdef IMAGO_SECURITY_ENABLED
    /** Load the cipher value. */
    pdu_read_byte( this_pdu, &cipher );

    /** Read the encrypted session key length. */
    pdu_read_short( this_pdu, &e_session_key_len );
    {
	unsigned char e_session_key[e_session_key_len];
	unsigned char session_key[e_session_key_len]; /** To be sure we have the room... */
	int len = e_session_key_len;
	/** 1) Extract encrypted session key and decrypt it. */
	if( pdu_read_string( this_pdu, e_session_key, e_session_key_len ) <  e_session_key_len ) {
	    /** Did not work... */
	    pdu_free( this_pdu );
	    return STATE_MACHINE_ID_INVALID;
	}
# ifdef _REENTRANT
	pthread_mutex_lock( &host_key_lock );
# endif
	/** Decrypt the session key. */
	len = rsa_private_decrypt( session_key, e_session_key, len, host_key->rsa );
# ifdef _REENTRANT
	pthread_mutex_unlock( &host_key_lock );
# endif
	if( len < 0 ) {
	    warning( 
		    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		    "@      ! ! ! ! WARNING: RSA DECRYPTION FAILURE ! ! ! !    @\n"
		    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		    " The server was not able to RSA decrypt the PDU from host: \n"
		    " '%s'\n"
		    " If your host key has changed, the server may be using the old one.\n",
		    " Request ignored.\n",
		    this_pdu->source_server 
		    );
	    /** Did not work... */
	    pdu_free( this_pdu );
	    return STATE_MACHINE_ID_INVALID;
	}
    
	/** Read the signature if present. */
	if( flags &  SEC_TYPE_SIGNED ) {
	    /** Read the encrypted signature len. */
	    pdu_read_short( this_pdu, &e_siglen );
	    {
		unsigned char e_sign[e_siglen];
# ifdef IMAGO_SIGN_ENABLED
		unsigned char digest[SHA_DIGEST_LENGTH];
		unsigned char sign[e_siglen]; /** To be sure we have the room... */
		int siglen;
		Key *src_key = Key_new();
# endif
		/** Extract encrypted signature. */
		if( pdu_read_string( this_pdu, e_sign, e_siglen ) <  e_siglen ) {
		    /** Did not work... */
		    pdu_free( this_pdu );
# ifdef IMAGO_SIGN_ENABLED
		    Key_free(src_key);
#  endif 
		    return STATE_MACHINE_ID_INVALID;
		}
# ifdef IMAGO_SIGN_ENABLED
		/** Load the source key. */
		if( load_key( this_pdu->source_server, src_key ) < 0 ) {
		    /** Ouch, we do not have the source key. 
			Request it and do not perform authentication. */
		    unsigned int seqid = get_seq_id();
		    /** Request it. */
		    if( rsa_host_key_send( this_pdu->source_server, seqid & 0xffff, SEC_TYPE_REQUEST ) ) {
			/** We where unable to generate an RSA key exchange PDU request. */
			warning( "Unable to initiate RSA key exchange.\n" );
			/** We let go through the the authentication will fail for sure. */
		    }
		}
#  ifdef _REENTRANT
		pthread_mutex_lock( &host_key_lock );
#  endif
		/** Decrypt the signature. */
		siglen = rsa_private_decrypt( sign, e_sign, e_siglen, host_key->rsa );
#  ifdef _REENTRANT
		pthread_mutex_unlock( &host_key_lock );
#  endif
		/** Get the SHA digest of the session key. */
		SHA1( session_key, SEC_SESSION_KEY_LENGTH, digest );
		/** Verify the authenticity of the message. */
		if( siglen < 0 || !src_key->dsa || DSA_verify(0, digest, sizeof(digest), sign, siglen, src_key->dsa) != 1) {
		    /** The authenticity of the sender could not be establish. */
		    warning( 
			    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
			    "@          ! ! ! ! WARNING: FALSIFIED PDU ! ! ! !         @\n"
			    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
			    " The authenticity of the PDU could not be established.     \n"
			    " Host: '%s'\n"
			    " Request ignored.\n",
			    this_pdu->source_server );
		    /** Free the source key. */
		    Key_free( src_key );
		    pdu_free( this_pdu );
		    return STATE_MACHINE_ID_INVALID;
		}
		Key_free( src_key );
# else
		warning( "The PDU has been signed, be we do not support signature...\n" );
		/** Verify the session key with the signature. */
# endif
	    }
	} 
#ifdef HIGH_SECURITY
	else {
	    warning( 
		    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		    "@        ! ! ! ! WARNING: SECURITY FAILURE ! ! ! !        @\n"
		    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
		    " The server did not sign the PDU.\n"
		    " Host: '%s'\n"
		    " Authenticity could not be established.\n"
		    " Request ignored.\n",
		    this_pdu->source_server );
	    pdu_free( this_pdu );
	    return STATE_MACHINE_ID_INVALID;
	}
#endif
	/** 3) Decrypt data using session key. */
	if( symetric_decrypt( this_pdu, session_key, cipher ) < 0 ) {
	    /** We could not decrypt the PDU. */
	    pdu_free( this_pdu );
	    return STATE_MACHINE_ID_INVALID;
	} 
    }
    /** Forward up the stack the PDU. */
    return imago_layer_ADArendezvous_up_stateid;
#else /** #ifdef IMAGO_SECURITY_ENABLED */
    /** The server does not suport security but the PDU is encrypted. */
    warning( "The PDU has been encrypted, but we do not support encyption.\nRequest ignored.\n");
    pdu_free( this_pdu );
    return STATE_MACHINE_ID_INVALID;
#endif /** #ifdef IMAGO_SECURITY_ENABLED */
}

/** Generique functions. */
state_id_t imago_layer_security_up( struct pdu_str *this_pdu ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu );
    if( this_pdu->flags & PDU_TYPE_SITP )
	return imago_layer_security_transport_up( this_pdu );
    else
	return imago_layer_security_control_up( this_pdu );
}


state_id_t imago_layer_security_down( struct pdu_str *this_pdu ) {
    debug( "%s(%p)\n", __PRETTY_FUNCTION__, this_pdu ); 
#ifdef IMAGO_SECURITY_ENABLED  
    if( this_pdu->flags & PDU_TYPE_SITP )
	return imago_layer_security_transport_down( this_pdu );
    else
	return imago_layer_security_control_down( this_pdu );
#else  /** #ifdef IMAGO_SECURITY_ENABLED  */
    if ( pdu_write_byte( this_pdu, 0 ) < 0 ) {
	this_pdu->errno = EPDU_MFRAME;
	return STATE_MACHINE_ID_INVALID;
    }
    return imago_layer_routing_down_stateid;
#endif /** #ifdef IMAGO_SECURITY_ENABLED */
}
/** */
