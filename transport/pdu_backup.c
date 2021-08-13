/** $RCSfile: pdu_backup.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: pdu_backup.c,v 1.7 2003/03/24 14:07:48 gautran Exp $ 
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>


#include "include.h"
#include "queue.h"
#include "frame.h"
#include "imago_pdu.h"
#include "pdu_backup.h"


#ifdef _PDU_BACKUP_USE_CACHE
#ifdef _REENTRANT
extern pthread_mutex_t pdu_table_lock;
#endif
/** Start internal function declarations. */
struct pdu_str *load_from_file( const char *, struct pdu_str * );
struct pdu_str *load_from_cache( const char *, struct pdu_str * );
struct pdu_str *load_from_disk( const char *, struct pdu_str * );
struct pdu_str *load_from_idcache( const int, struct pdu_str * );
struct pdu_str *load_from_iddisk( const int, struct pdu_str * );

int cancel_from_cache( const char * );
int cancel_from_disk( const char * );
int cancel_from_idcache( const int );
int cancel_from_iddisk( const int );

int save_to_disk( const char *, const int, struct pdu_str * );
/** Useful MACROs. */
#define BACKUP_RINDEX(L) ((L) & 0xFF)

#define BACKUP_KEY_TEMPLATE  IMAGO_PDU_SPOOL "/." IMAGO_PDU_SPOOL_PRE "%s" IMAGO_PDU_SPOOL_EXT
#define BACKUP_L_ID_TEMPLATE IMAGO_PDU_SPOOL "/." IMAGO_PDU_SPOOL_PRE "%d" IMAGO_PDU_SPOOL_EXT
#define BACKUP_FILE_TEMPLATE IMAGO_PDU_SPOOL "/" IMAGO_PDU_SPOOL_PRE "%2$d%1$c%3$s" IMAGO_PDU_SPOOL_EXT, IMAGO_PDU_SPOOL_SEP
#define BACKUP_BFILE_TEMPLATE IMAGO_PDU_SPOOL "/" IMAGO_PDU_SPOOL_PRE "%2$d%1$c%3$s", IMAGO_PDU_SPOOL_SEP
#define BACKUP_LINK_TEMPLATE IMAGO_PDU_SPOOL_PRE "%2$d%1$c%3$s" IMAGO_PDU_SPOOL_EXT, IMAGO_PDU_SPOOL_SEP

/** Data Structure. */
#define BACKUP_VALID 0x01
#define BACKUP_INVALID 0x00

struct bentry {
    char key[BACKUP_NAME_MAX];
    int l_id; /** The logical backup id. */
    struct pdu_str *b_pdu;
    char flags;
};

struct bentry bcache[MAX_PDU_BACKUP_CACHE_ENTRY];
#ifdef _REENTRANT
static pthread_mutex_t bcache_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/** End declaration. */

/** Start Interface Function definition. */
int backup_save( const char *key, const struct pdu_str *b_pdu ) {
    /** Compute the real id. */
    int r_id = strhash_np( key, MAX_PDU_BACKUP_CACHE_ENTRY );
    debug("%s(%s,%p)\n", __PRETTY_FUNCTION__, key, b_pdu );
#ifdef _REENTRANT
    pthread_mutex_lock( &bcache_lock );
#endif 
    /** Check if the cache hold the backup or hold a backup. */
    if( bcache[r_id].flags & BACKUP_VALID ) {
	bcache[r_id].flags = BACKUP_INVALID;
	/** This is a used slot. 
	    Make sure this is not the same backup. 
	    If so, we just write on top of it.
	    Else, we save to the disk the original
	    and replace it with the new one. */
	if( strncmp(bcache[r_id].key, key, BACKUP_NAME_MAX) != 0 ) {
	    /** This is not the same... Write the old backup to the disk. */
	    if( save_to_disk(bcache[r_id].key, bcache[r_id].l_id, bcache[r_id].b_pdu) < 0 ) {
		/** The save operation failed. */
		r_id = -1;
		bcache[r_id].flags = BACKUP_VALID;
	    } 
	}
    }
    /** At this point, the slot can be re-used only if the real id is not < 0. */
    if( r_id >= 0 ) {
	/** First, clone the PDU. */
	struct pdu_str *c_pdu = pdu_clone( b_pdu, bcache[r_id].b_pdu );
	/** If the clone succeed, continue. */
	if( c_pdu != NULL ) {
	    /** Store the clone address. */
	    bcache[r_id].b_pdu = c_pdu;
	    /** Copy the key. */
	    strncpy( bcache[r_id].key, key, BACKUP_NAME_MAX );
	    /** Set the flags. */
	    bcache[r_id].flags = BACKUP_VALID;
	    /** And compute the new logical ID to be returned. */
	    bcache[r_id].l_id =  r_id + ((bcache[r_id].l_id + 0x100) & ~0xff);
	    /** If the logical id wrap, just reset the conflict part. */
	    if( bcache[r_id].l_id < 0 ) bcache[r_id].l_id = r_id;
	    /** Here we just reuse the real id for the logical id because we are lazy... */
	    r_id = bcache[r_id].l_id;
	} else r_id = -1;
    }
#ifdef _REENTRANT
    pthread_mutex_unlock( &bcache_lock );
#endif 
    /** The real index indicate the outcome.
	If everything succeed, this is the logical id. */
    return r_id;
}
/** */
struct pdu_str *backup_load( const char *key, struct pdu_str *l_pdu ) {
    struct pdu_str *b_pdu;
    debug("%s(%s,%p)\n", __PRETTY_FUNCTION__, key, l_pdu );
    b_pdu = load_from_cache( key, l_pdu );
    debug("%s(%s,%p)\n", __PRETTY_FUNCTION__, key, b_pdu );
    /** First, try to load from the cache. */
    if( b_pdu ) return b_pdu;
    /** If not found in the cache, then it must be on disk. */
    return load_from_disk(key, l_pdu);
}
struct pdu_str *backup_idload( const int id, struct pdu_str *l_pdu ) {
    struct pdu_str *b_pdu;
    debug("%s(%d,%p)\n", __PRETTY_FUNCTION__, id, l_pdu );
    b_pdu = load_from_idcache( id, l_pdu );
    /** First, try to load from the cache. */
    if( b_pdu ) return b_pdu;
    /** If not found in the cache, then it must be on disk. */
    return load_from_iddisk( id, l_pdu );
}
/** */
/** Used to destroy (or cancel) a specific backup entry. */
int backup_cancel( const char *key ) {
    /** We first try canceling the backup in the cache. */
    int ret = cancel_from_cache( key );
    debug("%s(%s)\n", __PRETTY_FUNCTION__, key);
    /** If it fails, we cancel the backup on disk. */
    if( ret < 0 ) ret = cancel_from_disk( key );
    /** Return the result. */
    return ret;
}
int backup_idcancel( const int l_id ) {
    /** We first try canceling the backup in the cache. */
    int ret = cancel_from_idcache( l_id );
    debug("%s(%d)\n", __PRETTY_FUNCTION__, l_id );
    /** If it fails, we cancel the backup on disk. */
    if( ret < 0 ) ret = cancel_from_iddisk( l_id );
    /** Return the result. */
    return ret;
}

/** This function is bi-lateral.
    It saves all cached backups to the disk, and at the same time, 
    reload them all from the disk to the cache.
    Used both at start up, and shutdown.
*/
void backup_sync() { return; }
/** End Interface Function definition. */

/** Start internal Function definition. */

struct pdu_str *load_from_cache( const char *key, struct pdu_str *l_pdu ) {
    struct pdu_str *b_pdu = NULL;
    /** Compute the real id. */
    int r_id = strhash_np( key, MAX_PDU_BACKUP_CACHE_ENTRY );
    debug("%s(%s,%p)\n", __PRETTY_FUNCTION__, key, l_pdu );
#ifdef _REENTRANT
    pthread_mutex_lock( &bcache_lock );
#endif 
    /** If the cache does not hold the backup, then return failure. */
    if( (bcache[r_id].flags & BACKUP_VALID) && strncmp(bcache[r_id].key, key, BACKUP_NAME_MAX) == 0 ) {
	/** This is the real backup.
	    Unfortunatly, we MUST clone it so our copy does not get corrupted by the caller... */
	b_pdu = pdu_clone( bcache[r_id].b_pdu, l_pdu );
    }
#ifdef _REENTRANT
    pthread_mutex_unlock( &bcache_lock );
#endif 
    return b_pdu;
}
struct pdu_str *load_from_idcache( const int l_id, struct pdu_str *l_pdu ) {
    struct pdu_str *b_pdu = NULL;
    /** Compute the real id. */
    int r_id = BACKUP_RINDEX(l_id);
    debug("%s(%d,%p)\n", __PRETTY_FUNCTION__, l_id, l_pdu );
#ifdef _REENTRANT
    pthread_mutex_lock( &bcache_lock );
#endif 
    /** If the cache does not hold the backup, then return failure. */
    if( (bcache[r_id].flags & BACKUP_VALID) && (bcache[r_id].l_id == l_id) ) {
	/** This is the real backup.
	    Unfortunatly, we MUST clone it so our copy does not get corrupted by the caller... */
	b_pdu = pdu_clone( bcache[r_id].b_pdu, l_pdu );
    }
#ifdef _REENTRANT
    pthread_mutex_unlock( &bcache_lock );
#endif 
    return b_pdu;
}
/** */
struct pdu_str *load_from_file( const char *filename, struct pdu_str *l_pdu ) {
    char buf[FRAME_SIZE];
    struct pdu_str *b_pdu = l_pdu;
    int fd, amount;
    /** The user my give us a PDU to load the backup to. */
    if( b_pdu == NULL ) b_pdu = pdu_new();
    else frame_destroy( &b_pdu->frames );
    /** Still, make sure we do have something to load to. */
    if( b_pdu == NULL ) return NULL;
    /** Now we have the PDU structure, we consider it clean ! ! ! */
    /** Once we have the file name, open it. */
    if( (fd = open( filename, O_RDONLY )) < 0 ) {
	/** There is an error. */
	/** If we created the PDU, destroy it. */
	if( l_pdu == NULL ) pdu_free( b_pdu );
	/** Return failure. */
	return NULL;
    }
#ifdef _REENTRANT
    /** We HAVE to lock the PDU table because the restored structure will be not valid until its self
	pointer really pointes to itself. */
    pthread_mutex_lock( &pdu_table_lock );
#endif 
    /** Read the PDU control block. */
    amount = read( fd, b_pdu, sizeof(struct pdu_str) );
    b_pdu->self = b_pdu;
#ifdef _REENTRANT
    pthread_mutex_unlock( &pdu_table_lock );
#endif 
    /** Do not forget to reset some bad pointers... */
    queue_init( &b_pdu->frames );
    if( amount < sizeof(struct pdu_str) ) {
	/** If we did not read as much as the size of the structure, destroy the PDU. */
	if( l_pdu == NULL ) pdu_free(b_pdu);
	b_pdu = NULL;
    } else {
	/** Good ! Start reading the frames. */
	while( (amount = read(fd, buf, sizeof(buf))) > 0 ) {
	    if( frame_append_string( &b_pdu->frames, buf, amount ) < 0 ) {
		/** We ran out of frames... */
		if( l_pdu == NULL ) pdu_free(b_pdu);
		b_pdu = NULL;
		break;
	    }
	}
    }
    /** Close the file. */
    close( fd );
    /** Return the restored pdu. */
    return b_pdu;
}

struct pdu_str *load_from_disk( const char *key, struct pdu_str *l_pdu ) {
    char pathname[FILENAME_MAX]; /** Should be long enough for a file name. */
    struct pdu_str *b_pdu = l_pdu;
    debug("%s(%s,%p)\n", __PRETTY_FUNCTION__, key, l_pdu );
    /** The user my give us a PDU to load the backup to. */
    if( b_pdu == NULL ) b_pdu = pdu_new();
    /** Still, make sure we do have something to load to. */
    if( b_pdu == NULL ) return NULL;
    /** Now we have the PDU structure, we consider it clean ! ! ! */
    /** Create the file name for this backup. */
    snprintf( pathname, sizeof(pathname)-1, BACKUP_KEY_TEMPLATE, key );
    /** Once we have the file name, load the PDU. */
    if( load_from_file(pathname, b_pdu) == NULL ) {
	/** If we created the PDU, destroy it. */
	if( l_pdu == NULL ) pdu_free( b_pdu );
	b_pdu = NULL;
    } else debug( "PDU successfully restored: '%s' (%d bytes).\n", key, pdu_size(b_pdu) );
    /** Return the restored pdu. */
    return b_pdu;
}
struct pdu_str *load_from_iddisk( const int l_id, struct pdu_str *l_pdu ) {
    char pathname[FILENAME_MAX]; /** Should be long enough for a file name. */
    struct pdu_str *b_pdu = l_pdu;
    debug("%s(%d,%p)\n", __PRETTY_FUNCTION__, l_id, l_pdu );
    /** The user my give us a PDU to load the backup to. */
    if( b_pdu == NULL ) b_pdu = pdu_new();
    /** Still, make sure we do have something to load to. */
    if( b_pdu == NULL ) return NULL;
    /** Now we have the PDU structure, we consider it clean ! ! ! */
    /** Create the file name for this backup. */
    snprintf( pathname, sizeof(pathname)-1, BACKUP_L_ID_TEMPLATE, l_id );
    /** Once we have the file name, load the PDU. */
    if( load_from_file(pathname, b_pdu) == NULL ) {
	/** If we created the PDU, destroy it. */
	if( l_pdu == NULL ) pdu_free( b_pdu );
	b_pdu = NULL;
    } else debug( "PDU successfully restored: '%d' (%d bytes).\n", l_id, pdu_size(b_pdu) );
    /** Return the restored pdu. */
    return b_pdu;
}
/** */
int save_to_disk( const char *key, const int l_id, struct pdu_str *b_pdu ) {
    /** Write the valid b_pdu structure onto the hard drive. */
    int ret = -1, fd;
    char pathname[FILENAME_MAX]; /** Should be long enough for a file name. */    
    char linkname[FILENAME_MAX]; /** Should be long enough for a file name. */    
    /** Create the file name for this backup. */
    snprintf( pathname, sizeof(pathname)-1, BACKUP_FILE_TEMPLATE, l_id, key );
    /** Once we have the file name, create the file. */
    if( (fd = open( pathname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR )) < 0 ) {
	/** There is an error. */
	return -1;
    }
    /** Dump the PDU control block first. */
    if( write( fd, b_pdu, sizeof(struct pdu_str) ) < 0 ) goto error;
    /** Dump the frames next. */
    if( pdu_send_frame( fd, b_pdu ) < 0 ) goto error;
    /** Now, create the links to the backup file. */
    snprintf( linkname, sizeof(linkname)-1, BACKUP_LINK_TEMPLATE, l_id, key );
    /** Create the symlink for the Logical id. */
    snprintf( pathname, sizeof(pathname)-1, BACKUP_L_ID_TEMPLATE, l_id );
    unlink( pathname );
    symlink( linkname, pathname );
    /** Create the symlink for the hash key. */
    snprintf( pathname, sizeof(pathname)-1, BACKUP_KEY_TEMPLATE, key );
    unlink( pathname );
    symlink( linkname, pathname );
    /** All set. */
    debug( "PDU sucessfully backed up: '%s' (%d bytes).\n", linkname, pdu_size(b_pdu) );
    ret = 1;
 error:
  /** Close the file. */ 
  close( fd );
  return ret;
}

/** */
int cancel_from_cache( const char *key ) {
    /** Compute the real id. */
    int r_id = strhash_np( key, MAX_PDU_BACKUP_CACHE_ENTRY );
    debug("%s(%s)\n", __PRETTY_FUNCTION__, key);
#ifdef _REENTRANT
    pthread_mutex_lock( &bcache_lock );
#endif 
    /** If the cache does not hold the backup, then return failure. */
    if( (bcache[r_id].flags & BACKUP_VALID) && strncmp(bcache[r_id].key, key, BACKUP_NAME_MAX) == 0 ) {
	/** This is the real backup. Cancel it. */
	if( bcache[r_id].b_pdu ) pdu_free( bcache[r_id].b_pdu );
	bcache[r_id].b_pdu  = NULL;
	/** Invalidate the entry. */
	bcache[r_id].flags = BACKUP_INVALID;
    } else r_id = -1;
#ifdef _REENTRANT
    pthread_mutex_unlock( &bcache_lock );
#endif 
    /** The real index indicate the outcome. */
    return r_id;
}
int cancel_from_idcache( const int l_id ) {
    /** Compute the real id. */
    int r_id = BACKUP_RINDEX(l_id);
#ifdef _REENTRANT
    pthread_mutex_lock( &bcache_lock );
#endif 
    /** If the cache does not hold the backup, then return failure. */
    if( (bcache[r_id].flags & BACKUP_VALID) && (bcache[r_id].l_id == l_id) ) {
	/** This is the real backup. Cancel it. */
	if( bcache[r_id].b_pdu ) pdu_free( bcache[r_id].b_pdu );
	bcache[r_id].b_pdu  = NULL;
	/** Invalidate the entry. */
	bcache[r_id].flags = 0;
    } else r_id = -1;
#ifdef _REENTRANT
    pthread_mutex_unlock( &bcache_lock );
#endif 
    /** The real index indicate the outcome. */
    return r_id;
}
/** */
/** The backup file structure is such that we have one file which name contains both, 
    Logical ID and the hash Key od the backup, and two links. 
    The first link is made from the logical ID, while the second link is made out of
    the hash key.
    When we want to remove the backup file, we extract the l_id and key from the 
    file name, then delete both links along with the backup file.
    Like this, nothing is left onto the hard drive. */
int cancel_backup_file( const char *filename ) {
    /** The pathname must be the real file name and not one of the links. */
    char pathname[FILENAME_MAX]; /** Should be long enough for a file name. */
    int l_id = atoi( &filename[sizeof(IMAGO_PDU_SPOOL_PRE)-1] ); /** the logical id comes right away. */
    const char *ptr = strchr( filename, IMAGO_PDU_SPOOL_SEP )+1; /** At the end of the name comes the key plus the extentionh. */
    char key[BACKUP_NAME_MAX];
    memset( key, 0, BACKUP_NAME_MAX );
    memcpy( key, ptr, strlen(ptr)-(sizeof(IMAGO_PDU_SPOOL_EXT)-1) );
    /** Now we have both, Logical ID and hash key. 
	Cancel everything. */
    /** Create the file name for the logical id link. */
    snprintf( pathname, sizeof(pathname)-1, BACKUP_L_ID_TEMPLATE, l_id );
    /** Delete the Logical id link. */
    unlink( pathname );
    /** Create the file name for the hash key link. */
    snprintf( pathname, sizeof(pathname)-1, BACKUP_KEY_TEMPLATE, key );
    /** Delete the hash key link. */
    unlink( pathname );
    /** Create the file name for the real backup. */
    snprintf( pathname, sizeof(pathname)-1, BACKUP_FILE_TEMPLATE, l_id, key );
    debug( "Key: '%s', l_id: %d, filename: '%s'\n", key, l_id, pathname );
    /** Delete the real backup file. */
    return unlink( pathname );
}

int cancel_from_disk( const char *key ) {
    char linkname[FILENAME_MAX]; /** Should be long enough for a file name. */
    char filename[FILENAME_MAX]; /** Should be long enough for a file name. */
    /** readlink(..) does not place a NULL at the end of the string... */
    memset( filename, 0, FILENAME_MAX );
    /** Create the file name for this backup. */
    snprintf( linkname, sizeof(linkname)-1, BACKUP_KEY_TEMPLATE, key );
    /** Then get the real file name. */
    if( readlink( linkname, filename, sizeof(filename)-1) <= 0 ) 
	return -1;
    /** Then, cancel the backup file. */
    return cancel_backup_file( filename );
}

int cancel_from_iddisk( const int l_id ) {
    char linkname[FILENAME_MAX]; /** Should be long enough for a file name. */
    char filename[FILENAME_MAX]; /** Should be long enough for a file name. */
    /** readlink(..) does not place a NULL at the end of the string... */
    memset( filename, 0, FILENAME_MAX );
    /** Create the file name for this backup. */
    snprintf( linkname, sizeof(linkname)-1, BACKUP_L_ID_TEMPLATE, l_id );
    /** Then get the real file name. */
    if( readlink( linkname, filename, sizeof(filename)-1) <= 0 ) 
	return -1;
    /** Then, cancel the backup file. */
    return cancel_backup_file( filename );
}
/** */
/** THE END */
#else //#ifdef _PDU_BACKUP_USE_CACHE


/** Imago PDU backup function (used by the ADA rendez-vous). */
int pdu_backup_create( register struct pdu_str *, const char * );
struct pdu_str *pdu_backup_restore( const char * );
struct pdu_str *pdu_backup_restore_from_id( int );
void pdu_backup_destroy( const char * );
void pdu_backup_destroy_from_id( int );





/** PDU Backup functions. */
struct bentry_str {
  int id;
  int len;
};

int backup_id = 0;

#define BACKUP_LIST IMAGO_PDU_SPOOL "/backup_list"
#define BACKUP_LIST_TMP IMAGO_PDU_SPOOL "/.backup_list"

#ifdef _REENTRANT
extern pthread_mutex_t pdu_table_lock;
#endif

#ifdef _REENTRANT
pthread_mutex_t backup_list_lock = PTHREAD_MUTEX_INITIALIZER;
#endif



int create_entry( const char *str ) {
  FILE *fp;
  struct bentry_str bid;
#ifdef _REENTRANT
  pthread_mutex_lock( &backup_list_lock );
#endif 
  bid.id = backup_id++;
  bid.len = strlen(str);

  fp = fopen( BACKUP_LIST, "a" );
  fwrite( &bid, sizeof(struct bentry_str), 1, fp );
  fwrite( str, bid.len, 1, fp );
  fclose( fp );
#ifdef _REENTRANT
  pthread_mutex_unlock( &backup_list_lock );
#endif 

  return bid.id;
}

void find_entry( int id, char *ptr, int len ) {
  FILE *fp;
  struct bentry_str bid;

  memset( ptr, 0, len );

#ifdef _REENTRANT
  pthread_mutex_lock( &backup_list_lock );
#endif 
  fp = fopen( BACKUP_LIST, "r" );
  for( ; fread( &bid, sizeof(struct bentry_str), 1, fp ); ) {
    char tmp[bid.len];
    fread( tmp, bid.len, 1, fp );
    tmp[bid.len] = 0;
    if( bid.id == id ) {
      memcpy( ptr, tmp, (len < bid.len)? len: bid.len );
      break;
    }
  }
  fclose( fp );
#ifdef _REENTRANT
  pthread_mutex_unlock( &backup_list_lock );
#endif 

}
void remove_entry( int id ) {
  FILE *rfp;
  FILE *wfp;
  struct bentry_str bid;
  int trigger = 0;
#ifdef _REENTRANT
  pthread_mutex_lock( &backup_list_lock );
#endif 
  rfp = fopen( BACKUP_LIST, "r" );
  wfp = fopen( BACKUP_LIST_TMP, "w" );
  if( rfp != NULL && wfp != NULL ) {
      for( ; fread( &bid, sizeof(struct bentry_str), 1, rfp ); ) {
	  char tmp[bid.len];
	  fread( tmp, bid.len, 1, rfp );
	  if( trigger && bid.id != id ) {
	      fwrite( &bid, sizeof(struct bentry_str), 1, wfp );
	      fwrite( tmp, bid.len, 1, wfp );
	  } else if( bid.id != id ) 
	      trigger = 1;
      }
      fclose( rfp );
      fclose( wfp );
      rename( BACKUP_LIST_TMP, BACKUP_LIST);
  } 
#ifdef _REENTRANT
  pthread_mutex_unlock( &backup_list_lock );
#endif 

}


/** The Backups are written on the disk. */
int pdu_backup_create( register struct pdu_str *this_pdu, const char *ext ) {
  char pathname[255]; /** Should be long enough for a file name. */
  int fd = 0;
  int id;
  debug( "%s(%p, \"%s\")\n", __PRETTY_FUNCTION__, this_pdu, ext ); 
  /** Generate an ID for this backup. */
  id = create_entry( ext );
  /** Create the file name for this backup. */
  snprintf( pathname, sizeof(pathname)-1, IMAGO_PDU_SPOOL "/" IMAGO_PDU_SPOOL_PRE "%s" IMAGO_PDU_SPOOL_EXT, ext );
  /** Once we have the file name, create the file. */
  if( (fd = open( pathname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR )) < 0 ) {
    /** There is an error. */
    this_pdu->errno = EPDU_FOPEN;
    return -1;
  }
  /** Write the ID first. */
  if( write( fd, &id, sizeof(int) ) < 0 ) {
    close( fd );
    this_pdu->errno = EPDU_FWRITE;
    remove_entry( id );
    return -1;    
  }

  /** Dump the PDU control block first. */
  if( write( fd, this_pdu, sizeof(struct pdu_str) ) < 0 ) {
    close( fd );
    this_pdu->errno = EPDU_FWRITE;
    remove_entry( id );
    return -1;
  }
  /** Dump the frames next. */
  if( pdu_send_frame( fd, this_pdu ) < 0 ) {
    close( fd );
    this_pdu->errno = EPDU_FWRITE;
    remove_entry( id );
    return -1;
  }
  /** Close the file. */ 
  close( fd );
  
  debug( "Backing up the PDU: %s (%d bytes).\n", ext, pdu_size(this_pdu) );
  /** Return success. */
  return id;
}

struct pdu_str *pdu_backup_restore_from_id( int id ) {
  char pathname[NAME_MAX+1]; /** Should be long enough for a file name. */
  find_entry( id, pathname, NAME_MAX+1 );
  if( strlen(pathname) <= 0 ) 
    return NULL;
  else
    return pdu_backup_restore( pathname );
}

struct pdu_str *pdu_backup_restore( const char *ext ) {
  char pathname[NAME_MAX+1]; /** Should be long enough for a file name. */
  int fd = 0, amount = 0, id = 0;
  struct pdu_str *this_pdu = pdu_new();
  char buf[FRAME_SIZE];
  debug( "%s(\"%s\")\n", __PRETTY_FUNCTION__, ext ); 
  /** Make sure we have a PDU... */
  if( this_pdu == NULL ) 
    return NULL;
  /** Create the file name for this backup. */
  snprintf( pathname, sizeof(pathname)-1, IMAGO_PDU_SPOOL "/" IMAGO_PDU_SPOOL_PRE "%s" IMAGO_PDU_SPOOL_EXT, ext );
  /** Once we have the file name, open it. */
  if( (fd = open( pathname, O_RDONLY )) < 0 ) {
    /** There is an error. */
    return NULL;
  }
  /** Read the backup ID first. */
  read( fd, &id, sizeof(int) );
#ifdef _REENTRANT
  /** We HAVE to lock the table because the restored structure will be not valid until its self
      pointer really pointes to itself. */
  pthread_mutex_lock( &pdu_table_lock );
#endif 
  /** Read the PDU control block. */
  amount = read( fd, this_pdu, sizeof(struct pdu_str) );
  this_pdu->self = this_pdu;
#ifdef _REENTRANT
  /** We HAVE to lock the table because the restored structure will be not valid until its self
      pointer really pointes to itself. */
  pthread_mutex_unlock( &pdu_table_lock );
#endif 
  /** Do not forget to reset some bad pointers... */
  queue_init( &this_pdu->frames );
  if( amount < sizeof(struct pdu_str) ) {
    /** If we did not read as much as the size of the structure, destroy the PDU. */
    pdu_free(this_pdu);
    close( fd );
    return NULL;
  }
  /** No, start reading the frames. */
  while( (amount = read(fd, buf, sizeof(buf))) > 0 ) {
    if( frame_append_string( &this_pdu->frames, buf, amount ) < 0 ) {
      /** We ran out of frames... */
      pdu_free(this_pdu);
      close( fd );
      return NULL;
    }
  }
  /** Closing the file. */
  close( fd );
  debug( "Restoring the PDU: %s (%d bytes).\n", ext, pdu_size(this_pdu) );
  /** The PDU has been restored with success... */
  return this_pdu;
}


void pdu_backup_destroy_from_id( int id ) {
  char pathname[NAME_MAX+1]; /** Should be long enough for a file name. */
  find_entry( id, pathname, NAME_MAX+1 );
  if( strlen(pathname) > 0 ) 
    pdu_backup_destroy( pathname );
}


void pdu_backup_destroy( const char *ext ) {
  char pathname[255]; /** Should be long enough for a file name. */
  int id, fd;
  debug( "%s(\"%s\")\n", __PRETTY_FUNCTION__, ext ); 
  /** Create the file name for this backup. */
  snprintf( pathname, sizeof(pathname)-1, IMAGO_PDU_SPOOL "/" IMAGO_PDU_SPOOL_PRE "%s" IMAGO_PDU_SPOOL_EXT, ext );
  if( (fd = open(pathname, O_RDONLY)) < 0 ) {
    /** No such file. */
    return;
  }
  /** Read the backup ID. */
  read( fd, &id, sizeof(int) );
  /** Do not forget to close the file... */
  close(fd);
  /** Remove the entry from the backup list. */
  remove_entry( id );
  /** Remove the file. */
  unlink( pathname );
}
/** */

/** Stub functions. */
int backup_save( const char *key, const struct pdu_str *b_pdu ) {
    return pdu_backup_create( (struct pdu_str*)b_pdu, key );
}
struct pdu_str *backup_load( const char *key, struct pdu_str *l_pdu ) {
    return pdu_backup_restore(key);
}
struct pdu_str *backup_idload( const int id, struct pdu_str *l_pdu ) {
    return pdu_backup_restore_from_id( id );
}
int backup_cancel( const char *key ) {
    pdu_backup_destroy( key );
    return 1;
}
int backup_idcancel( const int id ) {
    pdu_backup_destroy_from_id( id );
    return 1;
}



#endif

