/** $RCSfile: cipher.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: cipher.h,v 1.3 2003/03/21 13:12:03 gautran Exp $ 
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


#ifndef __SEC_AUX_H_
#define __SEC_AUX_H_

/** Suported ciphers. */
#define SEC_CIPHER_NONE         0x00       /* no encryption */
#define SEC_CIPHER_IDEA         0x01       /* IDEA CFB */
#define SEC_CIPHER_DES          0x02       /* DES CBC */
#define SEC_CIPHER_3DES         0x04       /* 3DES CBC */
#define SEC_CIPHER_BROKEN_RC4   0x08       /* Alleged RC4 */
#define SEC_CIPHER_BLOWFISH     0x10
#define SEC_CIPHER_2X           0x20
#define SEC_CIPHER_4X           0x40
#define SEC_CIPHER_8X           0x80

#define CIPHER_ENCRYPT  1
#define CIPHER_DECRYPT  0

#define INIT_VECTOR     "12345678"


/** RSA host key. */
typedef struct {
  /** Bit mask indicating the supported cipher. */
  unsigned char cipher_mask;
  /** This is the RSA host key. */
  RSA *rsa;
  /** This is the DSA host key. */
  DSA *dsa;
} Key;


Key *Key_new();
void Key_free( Key * );


int load_private_key( Key *, const char *, char * );
int save_private_key( Key *, const char *, char * );

int load_key( const char *, Key * );
int save_key( const char *, Key * );
int invalidate_key( const char *, Key * );
int check_save_key( const char *, Key * );

int put_bignum(struct pdu_str *, BIGNUM *);
int get_bignum(struct pdu_str *, BIGNUM **);
int read_bignum( char **, BIGNUM **);
int write_bignum( FILE *, BIGNUM *);

int do_crypto( struct pdu_str *, unsigned char *, int, unsigned int *, int );


#endif
