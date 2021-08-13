/** $RCSfile: cipher.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: cipher.c,v 1.5 2003/03/21 13:12:03 gautran Exp $ 
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/pem.h>


#include "include.h"
#include "imago_pdu.h"
#include "cipher.h"
#include "frame.h"

static const char known_hosts_file[] = { IMAGO_KNOWN_HOST_FILE };
/* Version identification string for IMAGO Security files. */
static const char imago_id_string[] = "IMAGO PRIVATE KEY FILE FORMAT 0.0\n";


static Key keystore[sizeof(int)*8]; /** 32 keys must be many enough. */
unsigned int keymap = 0;
# ifdef _REENTRANT
pthread_mutex_t keymap_lock = PTHREAD_MUTEX_INITIALIZER;
# endif


Key *Key_new() {
  Key *key = NULL;
  int i;
# ifdef _REENTRANT
  pthread_mutex_lock( &keymap_lock ); 
# endif
  for( i = 0; i < (sizeof(keymap) * 8); i++ ) {
    if( !(keymap & (1 << i)) ) {
      /** Not used key. */
      key = &keystore[i];
      /** Mark that key as used. */
      keymap |= (1 << i);
      /** Initialize the key. */
      memset( key, 0, sizeof(Key) );
      break;
    }
  }
# ifdef _REENTRANT
  pthread_mutex_unlock( &keymap_lock ); 
# endif
    return key;
}

void Key_free( Key *key ) {
  int i;
# ifdef _REENTRANT
  pthread_mutex_lock( &keymap_lock ); 
# endif
  /** Free the RSA and DSA components. */
  if( key->rsa ) RSA_free( key->rsa );
  if( key->dsa ) DSA_free( key->dsa );
  memset( key, 0, sizeof(Key) );

  for( i = 0; i < (sizeof(keymap) * 8); i++ ) {
    if( key == &keystore[i] ) {
      /** Mark as not used key. */
      keymap &= ~(1 << i);
      break;
    }
  }
# ifdef _REENTRANT
  pthread_mutex_unlock( &keymap_lock ); 
# endif
}

/**
 * Parses an RSA (number of bits, e, n) or DSA key from a string.  Moves the
 * pointer over the key.  Skips any whitespace at the beginning and at end.
 */
int read_key( Key *ret, char **cpp) {
  char *cp;
  int cipher;
  /** Skip leading whitespace. */
  for( cp = *cpp; *cp == ' ' || *cp == '\t'; cp++);

  /** First, read the cipher mask. */
  if( sscanf( cp, "%X", &cipher ) < 1 ) 
    return -1;
  ret->cipher_mask = cipher & 0xff;

  /** Skip the cipher mask. */
  while( !isspace(*cp) ) cp++;

  if( ret->rsa == NULL && (ret->rsa = RSA_new()) == NULL ) 
    return -1;

  if( read_bignum( &cp, &ret->rsa->n ) < 0 ) 
    return -1;

  if( read_bignum( &cp, &ret->rsa->e ) < 0 )
    return -1;

#ifdef IMAGO_SIGN_ENABLED
  /** Write the DSA key. */
  if( ret->dsa == NULL && (ret->dsa = DSA_new()) == NULL ) 
    goto end;

  /** Write the prime number. */
  if( read_bignum( &cp, &ret->dsa->p ) < 0 ) {
    DSA_free( ret->dsa );
    ret->dsa = NULL;
    goto end;
  }
  
  /** Write the 160-bit subprime. */
  if( read_bignum( &cp, &ret->dsa->q ) < 0 ) {
    DSA_free( ret->dsa );
    ret->dsa = NULL;
    goto end;
  }
   
  /** Write the generator of subgroup. */
  if( read_bignum( &cp, &ret->dsa->g ) < 0 ) {
    DSA_free( ret->dsa );
    ret->dsa = NULL;
    goto end;
  }
   
  /** Write the public key. */
  if( read_bignum( &cp, &ret->dsa->pub_key ) < 0 ) {
    DSA_free( ret->dsa );
    ret->dsa = NULL;
    goto end;
  }
   
  /** */
 end:
#endif
  /** Return results. */
  *cpp = cp;
  return 1;
}



int load_key( const char *host_name, Key *key ) {
  FILE *fd;
  char line[8192];
  int linenum = 0;
  char *cp, *cp2;
  
  debug( "%s(%s, %p)\n", __PRETTY_FUNCTION__, host_name, key ); 
  /** Open the file containing the list of known hosts. */
  fd = fopen(known_hosts_file, "r");
  if (!fd)
    return -1;

  /** Go through the file. */
  while( fgets( line, sizeof(line), fd) ) {
    cp = line;
    linenum++;
    
    /** Skip any leading whitespace */
    for (; *cp == ' ' || *cp == '\t'; cp++);

    /** Skip comments and empty lines. */
    if (!*cp || *cp == '#' || *cp == '\n')
      continue;
    
    /** Find the end of the host name portion. */
    for( cp2 = cp; *cp2 && *cp2 != ' ' && *cp2 != '\t'; cp2++ );

    /** Check if the host name matches. */
    if( strncasecmp(host_name, cp, (int)(cp2 - cp)) != 0)
      continue;

    /** Got a match.  Skip host name. */
    cp = cp2;

    /**
     * Extract the key from the line.  This will skip any leading
     * whitespace.  Ignore badly formatted lines.
     */
    if( read_key(key, &cp) >= 0 ) {
      fclose( fd );
      return 0;
    }
  }
  /** Clear variables and close the file. */
  fclose(fd);

  return -1;
}
int save_key( const char *host_name, Key *key ) {
  FILE *fd;
  debug( "%s(%s, %p)\n", __PRETTY_FUNCTION__, host_name, key ); 

  /** Error if there is not RSA key. */
  if( !key->rsa ) 
    return -1;

  /** Open the file containing the list of known hosts. */
  fd = fopen(known_hosts_file, "a"); /** "a" for appending to the end of the file. */
  if (!fd)
    return -1;

  /** Write the host name first, */
  fprintf( fd, "%s ", host_name );
  
  /** Write the cipher mask. */
  fprintf( fd, "%X ", key->cipher_mask );

  /** Write the modulus big number. */
  write_bignum( fd, key->rsa->n );

  fprintf( fd, " " );
  /** Write the exponent big number. */
  write_bignum( fd, key->rsa->e );

  /** Write the DSA key if present. */
  if(  key->dsa ) {
    fprintf( fd, " " );
    /** Write the prime number. */
    write_bignum( fd, key->dsa->p );

    fprintf( fd, " " );
    /** Write the 160-bit subprime. */
    write_bignum( fd, key->dsa->q );
    
    fprintf( fd, " " );
    /** Write the generator of subgroup. */
    write_bignum( fd, key->dsa->g );
    
    fprintf( fd, " " );
    /** Write the public key. */
    write_bignum( fd, key->dsa->pub_key );
    /** */
  }
    /** Write the end of line. */
  fprintf( fd, "\n" );

  fclose( fd );
  return 0;
}
int invalidate_key( const char *host_name, Key *key ) {
  debug( "%s(%s, %p)\n", __PRETTY_FUNCTION__, host_name, key ); 



  return -1;
}

int check_save_key( const char *host_name, Key *test_key ) {
  Key *key = Key_new();
  debug( "%s(%s, %p)\n", __PRETTY_FUNCTION__, host_name, key ); 

  /** First, try to load the test key. */
  if( load_key( host_name, key ) < 0 ) {
    /** The host key was not found at all, consider it as checked and save it. */
    return save_key( host_name, test_key );
  } else {
    /** The test key has been loaded, compare it. */
    /** Compare the public modulus. */
    if( BN_cmp(test_key->rsa->n, key->rsa->n) ) {
      /** Not the same... */
      goto error;
    }
    /** Compare the public exponents. */
    if( BN_cmp(test_key->rsa->e, key->rsa->e) ) {
      /** Not the same... */
      goto error;
    }
    /** All the same for RSA. */
#ifdef IMAGO_SIGN_ENABLED
    /** Comapre the DSA key as well... */
    /**
     * TO DO: compare the DSA keys.
     */
#endif
    /** clean all up. */
    Key_free( key );
    return 0;
  }
 error:
  /** clean all up. */
  Key_free( key );
  return -1;
} 

/*
 * Stores an BIGNUM in the buffer with a 2-byte msb first bit count, followed
 * by (bits+7)/8 bytes of binary data, msb first.
 */
int put_bignum(struct pdu_str *this_pdu, BIGNUM *value) {
  int bits = BN_num_bits(value);
  int bin_size = (bits + 7) / 8;
  unsigned char buf[bin_size];
  int oi;
  
  /** Get the value of it in binary */
  oi = BN_bn2bin(value, buf);
  if( oi != bin_size ) {
    warning( "put_bignum: BN_bn2bin() failed: oi %d != bin_size %d",
		 oi, bin_size);
    return -1;
  }
  
  /** Store the binary data. */
  if( pdu_write_string( this_pdu, (char *)buf, oi) < 0 ) {
    return -1;
  }
  /** Store the number of bits. */
  if( pdu_write_short( this_pdu, bits ) < 0 ) {
    /** Remove the previous string we wrote. */
    pdu_read_string( this_pdu, (char *)buf, oi );
    return -1;
  }

  /** */
  return 0;
}

/*
 * Stores an BIGNUM in the buffer with a 2-byte msb first bit count, followed
 * by (bits+7)/8 bytes of binary data, msb first.
 */
int get_bignum(struct pdu_str *this_pdu, BIGNUM **value) {
  short bits, bytes;
  /* Get the number for bits. */
  pdu_read_short( this_pdu, &bits );

  /* Compute the number of binary bytes that follow. */
  bytes = (bits + 7) / 8;
  if (bytes > 8 * 1024) {
    warning( "%s: cannot handle BN of size %d\n", __PRETTY_FUNCTION__, bytes);
    return -1;
  }
  /** Read the big number. */
  {
    unsigned char bin[bytes];
    if( pdu_read_string( this_pdu, bin, bytes ) < bytes ) {
      warning( "%s: input buffer too small\n",  __PRETTY_FUNCTION__ );
      return -1;
    }
    if( *value == NULL ) {
      if( (*value = BN_new()) == NULL ) {
	warning( "%s: Unable to allocate a new BN\n",  __PRETTY_FUNCTION__ );
	return -1;
      }
    }
    BN_bin2bn(bin, bytes, *value);
  }
  /** */
  return 0;
}

/*
 * Stores an BIGNUM in the buffer with a 2-byte msb first bit count, followed
 * by (bits+7)/8 bytes of binary data, msb first.
 */
int read_bignum( char **cpp, BIGNUM **value) {
  unsigned int bits;
  char old;
  char *cp;

  /** Skip any leading whitespace */
  for( cp = *cpp; *cp == ' ' || *cp == '\t'; cp++);

  /** Get the number of bits. */
  sscanf( cp, "%u", &bits );

  /** Skip the numbers. */
  for (; *cp >= '0' && *cp <= '9'; cp++);

  if( *value == NULL ) {
    if( (*value = BN_new()) == NULL ) {
      warning( "%s: Unable to allocate a new BN\n",  __PRETTY_FUNCTION__ );
      return -1;
    }
  }
  /** Skip any leading whitespace. */
  for( ; *cp == ' ' || *cp == '\t'; cp++ );
  
  /** Check that it begins with a decimal digit. */
  if( *cp < '0' || *cp > '9' )
    return -1;
  
  /** Save starting position. */
  *cpp = cp;
  
  /* Move forward until all decimal digits skipped. */
  for( ; *cp >= '0' && *cp <= '9'; cp++ );
  
  /** Save the old terminating character, and replace it by \0. */
  old = *cp;
  *cp = 0;
  
  /** Read the big number. */
  if( BN_dec2bn(value, *cpp) == 0 )
    return -1;

  /** Restore old terminating character. */
  *cp = old;
  
  /** Move beyond the number and return success. */
  *cpp = cp;
  
  /** */
  return 0;
}

/*
 * Stores an BIGNUM in the buffer with a 2-byte msb first bit count, followed
 * by (bits+7)/8 bytes of binary data, msb first.
 */
int write_bignum( FILE *fd, BIGNUM *value) {
  unsigned int bits;
  char *buf;
  bits = BN_num_bits(value);
  fprintf(fd, "%u", bits);
  
  if( (buf = BN_bn2dec(value)) == NULL ) {
    error( "write_bignum: BN_bn2dec() failed" );
    return -1;
  }
  fprintf(fd, " %s", buf);
  OPENSSL_free(buf);
  
  /** */
  return 0;
}

static int key_perm_ok(int fd, const char *filename) {
  struct stat st;

  if (fstat(fd, &st) < 0)
    return 0;
  /*
   * if a key owned by the user is accessed, then we check the
   * permissions of the file. if the key owned by a different user,
   * then we don't care.
   */
  if ((st.st_uid == getuid()) && (st.st_mode & 077) != 0) {
    error( "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    error( "@         WARNING: UNPROTECTED PRIVATE KEY FILE!          @");
    error( "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    error( "Permissions 0%3.3o for '%s' are too open.",
		 st.st_mode & 0777, filename);
    error( "It is recommended that your private key files are NOT accessible by others.");
    error( "This private key will be ignored.");
    return 0;
  }
  return 1;
}


int load_private_key( Key *prv, const char *filename, char *passphrase ) {
  FILE *fp;

  fp = fopen(filename, "r");
  if (fp == NULL)
    return -1;

  if (!key_perm_ok(fileno(fp), filename)) {
    error( "bad permissions: ignore key: %s", filename);
    fclose(fp);
    return -1;
  }
  /** Read RSA private key. */
  prv->rsa = PEM_read_RSAPrivateKey( fp, NULL, 0, passphrase );
#ifdef IMAGO_SIGN_ENABLED
  /** Read DSA private key. */
  prv->dsa = PEM_read_DSAPrivateKey( fp, NULL, 0, passphrase );
#endif
  
  fclose( fp );
  return 0;
}
int save_private_key( Key *prv, const char *filename, char *passphrase ) {
  FILE *fp;
  int fd;

  if( (fd = creat(filename, S_IRUSR | S_IWUSR )) < 0)
    return -1;

  if( (fp = fdopen( fd, "w" )) == NULL ) {
    close(fd );
    return -1;
  }
  /** Write RSA private key. */
  PEM_write_RSAPrivateKey( fp, prv->rsa, EVP_des_ede3_cbc(), NULL, 0, 0, passphrase );
#ifdef IMAGO_SIGN_ENABLED
  /** Write DSA private key. */
  PEM_write_DSAPrivateKey( fp, prv->dsa, EVP_des_ede3_cbc(), NULL, 0, 0, passphrase );
#endif
  
  fclose( fp );
  return 0;
}


/**
 * Encrypt or Decrypt the content of the PDU frames using the 32 bytes session key provided.
 * The encryption is executed using any of the cipher supported. If none is supported
 * an error is returned. 
 */
int do_crypto( struct pdu_str *this_pdu, unsigned char *session_key, int len, unsigned int *ciphers, int do_encrypt ) {
  /** Allow enough space in output buffer for additional block */
  char inbuf[FRAME_SIZE - EVP_MAX_IV_LENGTH], outbuf[FRAME_SIZE];
  int inlen, outlen;
  EVP_CIPHER *cipher_type;
  unsigned char iv[EVP_MAX_IV_LENGTH];
  EVP_CIPHER_CTX ctx;
  struct link_head_str encrypted;
  /** Initialize the encrypted frame queue. */
  queue_init( &encrypted );
  memcpy(iv, INIT_VECTOR, sizeof(iv));
  /** Get the cipher type. 
      They are tested in the preference order.
  */
  if( !*ciphers ) {
    /** No encryption. */
    cipher_type = EVP_enc_null();
    *ciphers = SEC_CIPHER_NONE;
# ifndef NO_3DES
  } else if( *ciphers & SEC_CIPHER_3DES ) {
    /** Default encryption. */
    cipher_type = EVP_des_ede3_cbc();
    *ciphers = SEC_CIPHER_3DES;
# endif
# ifndef NO_BF
  } else if( *ciphers & SEC_CIPHER_BLOWFISH ) {
    cipher_type = EVP_bf_cbc();
    *ciphers = SEC_CIPHER_BLOWFISH;
# endif
# ifndef NO_IDEA
  } else if( *ciphers & SEC_CIPHER_IDEA ) {
    cipher_type = EVP_idea_cfb();
    *ciphers = SEC_CIPHER_IDEA;
# endif
# ifndef NO_DES
  } else if( *ciphers & SEC_CIPHER_DES ) {
    cipher_type = EVP_des_cbc();
    *ciphers = SEC_CIPHER_DES;
# endif
# ifndef NO_RC4
  } else if( *ciphers & SEC_CIPHER_BROKEN_RC4 ) {
    cipher_type = EVP_rc4();
    *ciphers = SEC_CIPHER_BROKEN_RC4;
# endif
  } else {
    /** No encryption. */
    cipher_type = EVP_enc_null();
    *ciphers = SEC_CIPHER_NONE;
  }
#ifdef HIGH_SECURITY
  if( cipher_type == EVP_enc_null() || *ciphers == SEC_CIPHER_NONE ) {
    return -1;
  }
#endif
  /** Don't set key or IV because we will modify the parameters */
  EVP_CIPHER_CTX_init(&ctx);
  EVP_CipherInit(&ctx, cipher_type, NULL, NULL, do_encrypt );
  EVP_CIPHER_CTX_set_key_length(&ctx, len );
  /** We finished modifying parameters so now we can set key and IV */
  EVP_CipherInit( &ctx, NULL, session_key, iv, do_encrypt );

  for(;;) {
    inlen = pdu_read_string( this_pdu, inbuf, sizeof(inbuf) );
    if( inlen <= 0 ) break;
    if( !EVP_CipherUpdate(&ctx, outbuf, &outlen, inbuf, inlen) ) {
      /* Error */
      return -1;
    }
    frame_append_string( &encrypted, outbuf, outlen );
  }
  if( !EVP_CipherFinal(&ctx, outbuf, &outlen) ) {
    /* Error */
    return -1;
  }
  frame_append_string( &encrypted, outbuf, outlen );
  EVP_CIPHER_CTX_cleanup(&ctx);

  /** Then replace the none encrypted pdu with the encypted one. */
  /** Clear the old frames just in case there still any. */
  frame_destroy( &this_pdu->frames );
  /** Move the frames from the temporary list to the PDU list. */
  frame_move( &this_pdu->frames, &encrypted, frame_size(&encrypted) );
  /** Just in case some incomplete frames... */
  frame_destroy( &encrypted );
  /** */
  return 1;
}
