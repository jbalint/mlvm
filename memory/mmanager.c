/** $RCSfile: mmanager.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: mmanager.c,v 1.13 2003/03/21 13:12:03 gautran Exp $ 
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
/************************************************/
/*          mmanager.c                          */
/************************************************/

#include <stdio.h>
#include "system.h"
#include "opcode.h"
#include "engine.h"
#include "mmanager.h"


/** The new garbage collection uses a double buffer. */
static pthread_mutex_t gc_lock = PTHREAD_MUTEX_INITIALIZER;
static int gc_static[GC_STATIC];
/** */

int do_gc_start( ICBPtr, int **, int * );
int do_gc_stop( ICBPtr, int * );


int do_collection( ICBPtr );
int do_expansion( ICBPtr );
int do_contraction( ICBPtr );
 

/************************************************/
/* This function is called by memory manager    */
/* which passes the pointer of a lvm_thread     */
/* which requests for either garbage collection	*/
/* or memory expansion.                         */
/* It returns to the manager in cases of:       */
/* 1) finishing the requested job               */
/* 2) an error occurs                           */
/************************************************/

int mmanager(ICBPtr plt){

    if( plt->status == IMAGO_COLLECTION ) {
	debug("Imago %s starts %d's garbage collection\n", plt->name, plt->collections);
	return do_collection(plt);
    } else if( plt->status == IMAGO_EXPANSION ) {  // expansions
	debug("Imago %s starts %d's memory expansion\n", plt->name, plt->expansions);
	return do_expansion(plt);
    } else if( plt->status == IMAGO_CONTRACTION ) {  // Shrink the imago block. 
	debug("Imago %s contracting %d's memory expansion\n", plt->name, plt->expansions);
	return do_contraction(plt);
    } 
    return E_ILLEGAL_STATUS;
}

/************************************************/
/*						*/
/************************************************/

int do_contraction( ICBPtr plt ) {
    warning( "FIX ME: Implement '%s'\n", __PRETTY_FUNCTION__ );
    /**
     * TO DO: Shrink the imago block if required.
     */

    return E_SUCCEED;
}


/************************************************/
/*						*/
/************************************************/


int do_collection(ICBPtr plt){

    /************************************************/
    /*                                              */
    /* entry registers:                             */
    /*          plt->tt == top of root stack        */
    /*          plt->st == top of stack             */
    /*          plt->bb == top choice               */
    /*          plt->af == bottom of root stack     */
    /*          plt->nl == # of arguments           */
    /* working registers:                           */
    /*          cline == collecting line            */
    /*          root == a root                      */
    /*	    scanp == pointer to scan copy block	    */
    /* 	    freep == pointer to free copy block	    */
    /************************************************/

    /************************************************/
    /* Memory configuration:                        */
    /*          -----------------                   */
    /*          |     trail     |                   */
    /*          |               |<--- plt->af       */
    /*          |               |                   */
    /*          |---------------|<--- tt            */
    /*          |               |                   */
    /*          |               |                   */
    /*          |               |                   */
    /*          |               |                   */
    /*          |---------------|<--- st            */
    /*          |               |                   */
    /*   collect|     gc-area   |                   */
    /*          |               |                   */
    /*          |---------------|<--- cline         */
    /*          |               |                   */
    /*          |     Old       |                   */
    /*          |               |                   */
    /*          -----------------                   */
    /************************************************/

    /*	Registers	*/

    int	*scanp, *freep;
    int 	root;
    int	*const cline = addr(*plt->af) + 1;//  collecting line (never change)
    int	*const st = plt->st;	// stack pointer (never change)
    int	**tt = plt->tt;	// trail pointer (root stack top)
    int	*const copy_bottom;	// must be set before GC and never change
    int	*const stack_bottom = plt->stack_base;
    int	ax;	 	// temp var
    int	*rp;		// temp address nested in a root
    int *prevp;
    int	bx, ret; 
  
    // Before starting GC, we might meed some work to lock the shared copy block

    if( (ret = do_gc_start( plt, (int **)&copy_bottom, (int *)&ax )) != E_SUCCEED ) 
	return ret;

	// find previous generation in case some roots across many G's
	// we have to collect those roots and also make them retained
	// in the previous generation

	prevp = (int*) *plt->af - 1;
	while (prevp < plt->block_ceiling && tag(*prevp) != STR_TAG) prevp++;
	if (prevp == plt->block_ceiling) 
		prevp = plt->bb;
	else 
		prevp = addr(prevp); 

    // Starting GC_INIT:

    // (1) copy argument roots

    ax = plt->nl;	// # of arguments (has been set by engine)
    freep = copy_bottom;
    scanp = st - ax + 1;
    while( ax-- ) *freep++ = *scanp++;

    // (2) copy roots from the trail

    scanp = plt->af;			// get root bottom

    while( (root = gc_pop()) ) {
	if( tag(root) == STR_TAG || hit_mid(root) ){
        	       continue;
	}
 	else if( hit_previous(root) ) 
	{
 	    gc_push(*scanp);
	    *scanp-- = root;	// reserve for later GC or backtrack
	}
  	*freep = *(int*) root;			// copy a root
	*(int*) root = dest(freep);	// set copied address
	freep++;
    }
 
    // starting GC:

    scanp = copy_bottom;
  
 GC_SCAN:

    while( scanp < freep ) {
	// pointer shunting

	root = *scanp;

	while( is_var(root) ) {
 	    if( hit_old(root) ) {
 		*scanp++ = root;
		goto GC_SCAN;
	    } else if( hit_copied_var(root) ) {
 		*scanp++ = dest(root);
		goto GC_SCAN;
	    } else if( is_self_ref(root) ) {
 		*scanp = dest(scanp);		// make a self-ref
		*(int*) root = (int) scanp++; 	// forwarding
		goto GC_SCAN;
	    } else {
		root = *(int*)root;		// shunting
	    }
	}

	// check the root

	if( is_cst(root) ) {
	    *scanp++ = root;
	    goto GC_SCAN;
	} else if( is_fvl(root) ) {	// an already copied floating, 
	    scanp += 3;	// bypass
	    goto GC_SCAN;
	} else { // a list or a structure or a floating
	    rp = addr(root); // get term address
	    if( hit_old(rp) ) {
 		*scanp++ = root;
		goto GC_SCAN;
	    }

	    if( is_list(root) ) {
		ax = *(rp + 1); 	// get tail
		if (is_var(ax) && hit_copied_var(ax)){
 		    if ((int*) ax > scanp){ // tail to be scanned
 			*scanp++ = make_list(dest((int*)ax - 1));
			goto GC_SCAN;
		    }
		    else { // tail has been scanned, copy in case
 			*scanp++ = make_list(dest(freep));
			*freep++ = *rp++; // copy head
			*freep++ = ax; // bind to earlier
			goto GC_SCAN;
		    }
		}
		// otherwise we have to make a copy

		*scanp++ = make_list( dest(freep) );
		*freep++ = *rp;  		// copy head
		*(rp + 1) = (int) freep; 	// set tail forward
		*freep++ = ax;	  		// copy tail	
 		goto GC_SCAN;
		
	    } else {
		ax = *rp;		// get functor
		if(is_var(ax) && hit_copied_var(ax) ) {
 		    *scanp++ = make_str( dest(ax) );
		    goto GC_SCAN;
		}
		*rp = (int) freep;	// set str forward
		*scanp++ = make_str( dest(freep) );

		if( is_fvl(ax) ) {
		    *freep++ = ax;
		    *freep++ = *(rp + 1);	// copy w1
		    *freep++ = *(rp + 2);	// copy w2
		    goto GC_SCAN;
		} else {
 		    *freep++ = ax; 	// copy functor
		    for( bx = 1; bx <= arity(ax); bx++ ) {
			*freep++ = *(rp + bx); // copy args
		    }
		    goto GC_SCAN;
		}
	    }
	}
    }

    // Now, we should move copies back to the stack

    scanp = copy_bottom;
    rp = cline;
    while (scanp < freep) *rp++ = *scanp++;


    // Then we must mirror the arguments on the top of the stack
  
    ax = plt->nl;
    scanp = cline;
    while (ax--){
 	*rp++ = *scanp++;
    }

    // Finally we should set up new stack pointers
 
    rp--;				// new stack top;
    *tt-- = (int*) make_str(rp);	// new generation   

    plt->gl = plt->st = rp;
    plt->tt = tt;
    plt->collections++;

    // Before return, we might need some work to unlock the shared copy block
    do_gc_stop( plt, copy_bottom ); 

    return E_SUCCEED;
}
 
int do_expansion(ICBPtr plt){

    /* Block layout */
    /********************************************************
    
     -------------- <- block_ceiling
     | trail      |
     |------------|
     |            |
     |-----^------| <- stack_top
     |            |
     | stack      |
     |            |
     |------------| <- stack_base
     |            |
     |-----^------| <- name_top
     | name_table |
     |------------| <- name_base
     | code       |
     |------------|
     | proc_table |
     -------------- <- block_base

    *********************************************************/

    /* Registers	*/

    register int*	old_base;	// old block base pointer
    register int*	new_base;	// new block base pointer
    register int	old_size;	// old block size
    register int*	tp1;		// temp pointer
    register int*	tp2;		// temp pointer
    register int	adjustment; // expansion adjustment
    register int	distance;	// distance of old and new block
    register int	ax;
    register int	bx;
    debug("MEMORY EXPANSION at ST = %x, TT = %x\n", plt->st, plt->tt);
 
    old_base = plt->block_base;
    old_size = plt->block_ceiling - old_base;		// get old block size

    adjustment = (plt->expansions < MM_TURNING) ? old_size : MM_EXP_SIZE;

 
    new_base = (int*) realloc(old_base, (old_size + adjustment)*sizeof(int));

    if (!new_base) return E_MEMORY_OUT;

    distance = new_base - old_base;		// get distance of two blocks

    if (distance){
	// adjust base pointers

	plt->block_base = new_base;
	plt->block_ceiling += distance + adjustment;
	plt->stack_base += distance;
	plt->code_base += distance;
	plt->proc_base += distance;
	plt->name_base += distance*sizeof(int); // it is a char*
	plt->name_top += distance*sizeof(int);

	// adjust registers

	plt->pp += distance;
	plt->af += distance;
	plt->cp += distance;
	plt->cf += distance;
	plt->st += distance;
	plt->bb += distance;
	plt->b0 += distance;
	plt->gl += distance;
	plt->tt += distance;
    }
    else {
	plt->block_ceiling += adjustment;
    }
	debug("after expansion ST = %x\n", plt->st);

    plt->expansions++;

 
    if (distance){  	// block moved

	/****************************************************************/
	/*                                                              */
	/*                     move segments:                           */
	/* 1) procedure entry table must be adjusted                    */
	/* 2) code hast been moved but code entries should be adjusted  */
	/* 3) symbolic name table has been moved, no adjustment         */
	/* 4) active stack must be scanned and pointers be adjusted     */
	/****************************************************************/

 
	// 1) procedure table: adjust entries

	tp1 = plt->proc_base + 1;		// hit the entry slot
	tp2 = plt->code_base;			// get the proc_table stop point
	while (tp1 < tp2){
	    *tp1 += distance;
	    tp1 += 2;					// advance to the next entry
	}
 
	// 2) code: only adjust code entries

	tp1 = (int*) plt->name_base;	// get code stop point

	while (tp2 < tp1){				// tp2 starts from code_base
	    adjust_code(mtype(),
			mdirect(),
			madjust(),
			mhash());
	}

	// 3) symbolic table: nothing to do

	// 4) Linear scan the active stack
	// a) bypass constants, a special case is that when
	// a FVL_TAG is met, we have to bypass three cells (a floating value)
	// b) adjust all tag_on_pointer terms  which include var, str,
	// list, and control pointers such as cp, cf, bb, etc.,
	// in control frames

	tp1 = plt->stack_base;	 // tp1 points to stack_base
	tp2 = plt->st;			// get new active stack top
 
	while (tp1 <= tp2){
	    ax = *tp1;
	    bx = tag(ax);
	    if (cst_tag(bx)){
		if (is_fvl(ax)){ // bypass floating value
		    tp1 += 3;
		}
		else{
		    tp1++;
		}
	    }
	    else {
		ax = addr(ax) + distance;
		*tp1++ = ax | bx;
	    }
	}
    }

    // now we should move and adjust trail, no matter block moved or not

    tp1 = (int*) plt->tt;		// get trail top
    tp2 = tp1 + adjustment;	// new trail top
    plt->tt = tp2;	// save new trail top
    ax = old_size - (tp1 - new_base); // get active trail size

    /**
     * WARNING ! ! 
     * plt->tt points to a valide integer, not to nothing ! 
     */
    if (!distance){ // the same block, move trail only
	while (ax--) *(++tp2) = *(++tp1);
    }
    else {		// new block, move and adjust trail
	while(ax--){
	    bx = *(++tp1);
	    if (is_var(bx)) {
		*(++tp2) = (int)((int*) bx + distance);
	    }
	    else {	// must be a generation line
		bx = (int) addr(bx);
		*(++tp2) = make_str((int*) bx + distance);
	    }
	}
    }
 
    // now we should adjust TT's in CF chain and BB chain
    // old TT's have already been adjusted against distance

    tp1 = plt->cf;		// adjust TT in CF chain
    tp2 = plt->stack_base;
    while (tp1 > tp2) {
	*(tp1 - 2) += adjustment;
	tp1 = *tp1;	// next control frame
    }

    tp1 = plt->bb;		// adjust TT in BB chain
    while (tp1 > tp2) {
	*(tp1 - 2) += adjustment;
	tp1 = *(tp1 - 3);	// next choice frame
    }

    return E_SUCCEED;
}

/** Double buffer initialization code. */
int do_gc_start( ICBPtr plt, int **gc_ptr, int *gc_size ) {
    /** Compute how much room we will be requirering for the garbage collection. */
    /** Find the collection line. */
    *gc_size = ( plt->st - (int *)addr(*plt->af) ); 
    /** Maximum size needed for garbage collection is from gp to st. */
    if( *gc_size >= GC_STATIC ) {
	/** Not enough room... */
	/** In that case, try grabing some memory using the malloc. */
	if( ((*gc_ptr) = (int *)malloc(*gc_size * sizeof(int))) == NULL ) {
	    /** Out-of-memory... End-of-story. */
	    return E_MEMORY_OUT;
	}
    } else { 
	/** Plenty of room. */
	/** Try locking the static buffer. */
	if( pthread_mutex_trylock(&gc_lock) != 0 ) {
	    /** Someone else is using the buffer,
		delay our gc. */
	    return E_RESSOURCE_BUSY;
	}
	/** Otherwise, set gc_ptr to gc_static and we are off ! */
	*gc_ptr = gc_static;
	*gc_size = GC_STATIC;
    }
    /** We got the memory, clean it up so we avoid fancy core dump. */
    memset( *gc_ptr, 0, GC_STATIC * sizeof(int) );
    /** All good to go. */
    return E_SUCCEED;
}
/** */

/** Double buffer cleanup code. */
int do_gc_stop( ICBPtr plt, int *gc_ptr ) {
    /** Two cases can apply, either we used the static buffer for the garbage collection. 
	In that case, we need to release the lock.
	Or, we allocated an extra buffer and we must free it. 
    */
    if( gc_ptr == gc_static ) {
	/** Static buffer. */
	pthread_mutex_unlock( &gc_lock );
    } else {
	/** Dynamic buffer. */
	if( gc_ptr ) 
	    free( gc_ptr );
    }
    return E_SUCCEED;
}
/** */

/** THE END */
