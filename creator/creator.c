/** $RCSfile: creator.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: creator.c,v 1.31 2003/03/25 15:34:39 gautran Exp $ 
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
#include <unistd.h>

#include "system.h"
#include "opcode.h"
#include "engine.h"
#include "builtins.h"
#include "imago_icb.h"
#include "transport.h"

 
/*********************************************************/
/*                  creator.c                            */
/* these functions will create a new imago as required.  */
/* they will set ip->status to IMAGO_READY if succeeds   */
/* or IMAGO_ERROR if something wrong. In the later case, */
/* return value gives the error code.                    */
/*                                                       */
/* msg_init: initialize system messenger array           */
/* imago_loader: a temporary function for loading a      */
/* stationary imago.                                     */
/* imago_sys_loader: creating a worker by create/3.      */
/* imago_cloner: cloning a worker/messenger by clone/3.  */
/* imago_dispatcher: creating a messenger by dispatch/3. */
/* imago_unmarshaller: restoring a move-in imago.        */
/* other supplementary functions.                        */
/*********************************************************/

/* all loaders are based on the following block layout   */

/*************************************/
/*        Block Layout:              */
/* -------------- <- block_ceiling   */
/* | trail      |                    */
/* |-----v------|                    */
/* |            |                    */
/* |     ^      |                    */
/* |-----^------| <- stack_top       */
/* |            |                    */
/* | stack      |                    */
/* |            |                    */
/* |------------| <- stack_base      */
/* |     ^      |                    */
/* |-----^------| <- name_top        */
/* | name_table |                    */
/* |------------| <- name_base       */
/* | code       |                    */
/* |------------|                    */
/* | proc_table |                    */
/* -------------- <- block_base      */
/*************************************/

/** Default messenger home path relative to the system directory. */
/**
#ifndef _MSGER_HOME
# define MSGER_HOME  "../msger/"
#else
# define MSGER_HOME _MSGER_HOME
#endif
*/
#define MSGER_HOME _MSGER_HOME

#define ctype()		(opcode_tab[*tp2].type)

#define cdirect()	{ *tp1++ = *tp2++;}

#define cadjust()	{(int*) *tp1++ = (int*) *tp2++ + distance;}

#define chash() { *tp1++ = *tp2++;                                 \
		v = *tp1++ = *tp2++;                               \
 		(int*) *tp1++ = (int*) *tp2++ + distance;          \
 		while (v--){                                       \
		    *tp1++ = *tp2++;                               \
		    (int*) *tp1++ = (int*) *tp2++ + distance;      \
 		}                                                  \
	}


int arg_size(int, ICBPtr);
int* copy_arg(int, char*, ICBPtr);
int load_user_file(char*, int, ICBPtr);
int load_system_file(char*, int, ICBPtr);
int clone_block(ICBPtr, ICBPtr);
int load_code(FILE*, ICBPtr);
int name_table_expansion(ICBPtr, int*);
int release_msger(ICBPtr, char*);
int imago_sys_dispatcher(ICBPtr);

struct {
    char* fname;
    char* mname;
    ICBPtr ip;
} builtin_msger[] = {
    { "broadcast_messenger.lo", "$broadcastor", NULL},
    { "cod_messenger.lo", "$cod_messenger", NULL},
    { "multicast_messenger.lo", "$multicast_messenger", NULL},
    { "oneway_messenger.lo", "$oneway_messenger", NULL},
    { "paperboy_messenger.lo", "$paperboy", NULL},
    { "postman_messenger.lo", "$postman", NULL},
    { "sys_messenger.lo", "$sys_messenger", NULL},
    /** Last entry. */
    { NULL, NULL, NULL},
};

void msg_init(){
    FILE* fp;
    int magic, itype, security, ssize, psize, csize, snum, total, i;
    int* bp;
    ICBPtr ip;
    char fname[80];

    for (i = 0; builtin_msger[i].fname != NULL; i++){
	fname[0] = '\0';
	strcat(fname, MSGER_HOME);
	strcat(fname, builtin_msger[i].fname);

	if ((fp = fopen(fname, "r")) == NULL) continue;

	fscanf(fp, "%d%d%d%d%d%d", &magic, &itype, &security,
	       &ssize, &psize, &csize);

	// calculate and allocate a memory block
	// ssize (byte), psize*(term, entry), csize*(code)
 	// leave ~256 bytes for dynamic use

	snum = (ssize + NAME_EXP_SIZE)/sizeof(int); 
 
	total = snum + psize*2 + csize;

	if (itype != MESSENGER){
	    system_error("System messenger initialization", E_UNKNOWN_IMAGO);
	    exit(-1);
	}

	// here, we only allocate the actural size of the imago's
	// data and code, without the stack.

	if ((bp = (int*) malloc(total*sizeof(int))) == NULL){
	    system_error("System messenger initialization", E_MEMORY_OUT);
	    exit(-1);
        }

	ip = icb_new();      // get a new icb

	// decide address of each segment

	ip->block_base = bp;
	ip->block_ceiling = bp + total;
	ip->proc_base = bp;
	ip->code_base = bp + psize*2;
	ip->name_base = (char*) (bp + psize*2 + csize);
	ip->name_top = (char*) ip->name_base + ssize;
 
	// load code

	if (load_code(fp, ip) != E_SUCCEED){
	    system_error("System messenger initialization", E_UNKNOWN_OPCODE);
	    exit(-1);
	}
 
	// finish loading
	fclose(fp);

	// insert this ip to builtin_msger table
	builtin_msger[i].ip = ip;
    }		
}
void msg_cleanup() {
    int i;
    for (i = 0; builtin_msger[i].fname != NULL; i++){
	if( builtin_msger[i].ip )
	    icb_free( builtin_msger[i].ip );
	builtin_msger[i].ip = NULL;
    }
}

int imago_loader(ICBPtr ip) {
    // this loader is used (temporarily) to load a stationary
    // imago requested by mlvm-thread imago_in
    FILE* fp;
    char filename[FILENAME_MAX];
    int magic, itype, security, ssize, psize, csize, snum, total;
    int* bp;
    
    /** Construct the file name with the path. */
    snprintf( filename, FILENAME_MAX-1, "%s%s", ip->citizenship.name, ip->name );
    
    debug("%s(%p): Loading file '%s'\n", __PRETTY_FUNCTION__, ip, filename );

    if( (fp = fopen(filename, "r")) == NULL ) {
	warning("CREATOR: file %s does not exists\n", ip->name);
	return E_UNKNOWN_IMAGO;
    }

    fscanf(fp, "%d%d%d%d%d%d", &magic, &itype, &security,
	   &ssize, &psize, &csize);

    // calculate and allocate a memory block
    // ssize (byte), psize*(term, entry), csize*(code)
    // leave ~256 bytes for dynamic use

    snum = (ssize + NAME_EXP_SIZE)/sizeof(int); 
 
    total = snum + psize*2 + csize;

    if (itype != STATIONARY) {
	/** Don't forget to clean up EVERY thing. */
	fclose(fp);
	return E_UNKNOWN_IMAGO;
    } else
	total +=  INIT_STATIONARY_STACK*STACK_UNIT;

 
    if ((bp = (int*) malloc(total*sizeof(int))) == NULL) {
	/** Don't forget to clean up EVERY thing. */
	fclose(fp);
	return E_MEMORY_OUT;
    }

    // decide address of each segment

    ip->block_base = bp;
    ip->block_ceiling = bp + total;
    ip->proc_base = bp;
    ip->code_base = bp + psize*2;
    ip->name_base = (char*) (bp + psize*2 + csize);
    ip->name_top = (char*) ip->name_base + ssize;
    ip->stack_base = bp + snum + psize*2 + csize;

    // load code

    if (load_code(fp, ip) != E_SUCCEED) {
	/** Don't forget to clean up EVERY thing. */
	fclose(fp);
	return E_UNKNOWN_OPCODE;
    }
 
    // finish loading
    fclose(fp);

    // make ICB ready

    if( !ip->application )
	ip->application = get_unique_appid();
	
    strcpy(ip->name, "queen"); // rename this stationary imago as "queen"
    get_imago_localhost( ip->queen.name, sizeof( ip->queen.name) );
    ip->queen.host_id = gethostid_cache();

    ip->type = itype;
    ip->status = IMAGO_READY;
    ip->expansions = 0;
    ip->collections = 0;
    ip->pp = ip->cp = ip->code_base;		// starting entry
    ip->st = ip->bb = ip->b0 = ip->gl = ip->cf = ip->stack_base;
    ip->tt = (int**) (ip->block_ceiling - 1);
    ip->af = (int *)ip->tt;
    *ip->tt-- = (int*) make_str(ip->gl);

    // now we should log this newly created imago
    log_lock();
    log_alive(NULL, ip);
    log_unlock();

    return E_SUCCEED;
}

int imago_sys_loader(ICBPtr ip){
    // This loader is used to create a worker from builtin create/3
    // where ip points to parent imago who invoked the create/3.
    // Elligibility has been checked by the builtin predicate.
    // The loader should get a new ICB then do the same as the
    // above loader. Upon success, the sys_loader must insert both
    // ICB's (one for parent and one for the child) to ready queue.
    int v;
    ICBPtr cip;
    char fname[FILENAME_MAX];

    cip = icb_new();      // get a new icb
    if (!cip) return E_ICB_OUT;


    deref(v, ip->st - 1);
    v = con_value(v); // get imago symbolic name
    if (v < ASCII_SIZE){
	cip->name[0] = (char) v;
	cip->name[1] = '\0';
    }
    else { // copy child name
	strcpy(cip->name, name_ptr(ip->name_base, v));  
    }

    cip->application = ip->application;
    cip->queen       = ip->queen;
    cip->cin       = ip->cin;
    cip->cout      = ip->cout;
    cip->cerr      = ip->cerr;

    // get the file name to be loaded

    deref(v, ip->st - 2);
    v = con_value(v);  // access file name
    if (v < ASCII_SIZE){
	return E_UNKNOWN_IMAGO;  // impossible
    }

    /** Concatenate the queen home directory with the worker file name. */
    if( name_ptr(ip->name_base, v)[0] != FILE_SEP )
	snprintf( fname, FILENAME_MAX-1, "%s%s", ip->citizenship.name, name_ptr(ip->name_base, v) );
    else
	snprintf( fname, FILENAME_MAX-1, "%s", name_ptr(ip->name_base, v) );

    v = arg_size(*(ip->st), ip);

    if ((v = load_user_file(fname, v, cip))!= E_SUCCEED){
	icb_free(cip);
	return v;
    }


    // now we should establish the calling argument
    // the original argument is located at *(ip->st)
    // it must be copied onto cip->st, as the argument
    // could be arbitrarily complicated, we simulate GC
    // algorithm to set up the arg to destination

    /** We resized the Imago to have enough room to fit the arguments, the copy_arg shall not fail... */
    cip->st = copy_arg(*(ip->st), ip->name_base, cip);


    // now we should log this newly created imago
    log_lock();
    log_alive(NULL, cip);
    log_unlock();

    // we should insert this new guy into ready queue
    // and let the imago_creator to insert the parent imago back
    debug("SYS_LOADER: '%s'  has been created and logged\n", cip->name);
    icb_put_signal(engine_queue, cip);
    
    /** Release eventual waiting messengers. */
    release_waiting_msger();

    ip->status = IMAGO_READY;

    return E_SUCCEED;
}

int imago_cloner(ICBPtr ip){

    // This function is invoked by builtin predicate clone/3
    // where ip points to the parent ICB.  We have to check
    // the imago type to be cloned, a minor different handling
    // is required for different imago types. Stationary imago
    // can not be cloned, and this has been checked by the
    // clone/3 predicate.

    ICBPtr cip;
    int v, t;

    cip = icb_new();      // get a new icb
    if (!cip) return E_ICB_OUT;

    if ((cip->block_base = (int*) malloc((ip->block_ceiling - 
					  ip->block_base)*sizeof(int))) == NULL){
	icb_free(cip);
	return E_MEMORY_OUT;
    }

    if (ip->type == WORKER){
	deref(v, ip->st - 1);
	v = con_value(v); // get imago symbolic name
	// *** do we need to consider dynamic name
	// represented by a Prolog term?
	// if yes, then we should have a to_string
	// function to convert a term to a symbolic
	// name, such as to_string(v, cip, ip)
	if (v < ASCII_SIZE){// single char name
	    cip->name[0] = (char) v;
	    cip->name[1] = '\0';
	}
	else {// copy child name	
	    strcpy(cip->name, name_ptr(ip->name_base, v));  
	}
    }
    else{ // a messenger: copy sender's name
	strcpy(cip->name, ip->name);
    }

    clone_block(ip, cip);

    // now we should make the clone ICB ready
    // More importantly, we should release all messengers (if any) with
    // an instantiation 'cloned(clone_name)' to the Result of each
    // blocked messenger.

    cip->application = ip->application;
    cip->queen = ip->queen;  
    cip->type = ip->type;
    cip->status = IMAGO_READY;
    cip->cin  = ip->cin;
    cip->cout = ip->cout;
    cip->cerr = ip->cerr;
    cip->expansions = ip->expansions;
    cip->collections = ip->collections;
    cip->jumps = ip->jumps;
    if( is_worker(ip) ) 
	get_imago_localhost( cip->citizenship.name, sizeof(cip->citizenship.name) );
     else /** Messenger. */
	cip->citizenship = ip->citizenship;

	
    // now we should assign the return value to the var parameter
    // which has been checked in clone/2


    // assign return value MLVM_ZERO to parent
    v = search_index("parent", ip, ip->st);
    deref(t, ip->st);
    *(int*) t = make_con(v + ASCII_SIZE);
    bt_bbtrail(t, ip);

    //  deref(v, ip->st);
    //  *(int*) v = MLVM_ZERO;

    // assign return value MLVM_ONE to clone
    v = search_index("clone", cip, cip->st);
    deref(t, cip->st);
    *(int*) t = make_con(v + ASCII_SIZE);
    bt_bbtrail(t, cip);

    //    deref(v, cip->st);
    //  *(int*) v = MLVM_ONE;


    if( is_worker(cip) ){
	// now we should log this newly created imago if it is a worker
	log_lock();
	log_alive(NULL, cip);
	log_unlock();
	// release all messengers if any 
	// and instantiate R in each msger to cloned(cip->name)
	// this must be done before we change ip->status
	release_msger(ip, cip->name);
	/** At the same time, send a system registration messenger to the queen from the clone. */
	imago_sys_dispatcher( cip );
	/** Release the waiting_for_worker messengers. */
	release_waiting_msger();
    }

    // we should insert this new guy into ready queue
    // and let the imago_creator to insert the parent imago back

    icb_put_signal(engine_queue, cip);

    // before return, we should make the parent ready
    ip->status = IMAGO_READY;

    return E_SUCCEED;
}

int imago_dispatcher(ICBPtr ip){
    // this function is called by dispatch/3 for creating a messenger
    // there are two kind of messengers: system-defined and user-defined.
    // if (ip->nl == E_SUCCEED) then sys-defined else user-defined.
    // A user-defined messenger can be dispatched by stationary imago only, and 
    // this has been checked by the dispatch/3 predicate. A user-defined
    // messenger will be loaded from a local file. A sys-defined messenger
    // can be found in a pre-loaded messenger pool, a clone-like algorithm
    // is used to create such kind of messenger.

    int v;
    ICBPtr cip;
    char fname[FILENAME_MAX];


    cip = icb_new(); // get a new icb
    if (!cip) return E_ICB_OUT;

    // copy parent's name as sender's name
 	
    strcpy(cip->name, ip->name);   
    cip->application = ip->application;
    cip->queen       = ip->queen;
    cip->jumps       = ip->jumps;
    cip->cin         = ip->cin;
    cip->cout        = ip->cout;
    cip->cerr        = ip->cerr;
    get_imago_localhost( cip->citizenship.name, sizeof(cip->citizenship.name) );

    // get the file name to be loaded

    deref(v, ip->st - 2);
    v = con_value(v);  // access file name
    if (v < ASCII_SIZE){
	return E_UNKNOWN_IMAGO;  // impossible
    }
    snprintf( fname, FILENAME_MAX-1, "%s", name_ptr(ip->name_base, v) );

    /** Concatenate the queen home directory with the worker file name. */
    if( is_stationary(ip) && fname[0] != FILE_SEP && fname[0] != '$'  )
	snprintf( fname, FILENAME_MAX-1, "%s%s", ip->citizenship.name, name_ptr(ip->name_base, v) );

    deref(v, ip->st - 1); /** Destination */
    debug("In imago_dispatcher: %s dispatches %s to %s\n", 
	  cip->name, fname, (is_con(v))?name_ptr(ip->name_base,con_value(v)):"(unknown)");
 
    v = arg_size(*(ip->st), ip);

    if (ip->nl == E_SUCCEED){ // load sys-defined
	if ((v = load_system_file(fname, v, cip)) != E_SUCCEED) {
	    icb_free(cip);
	    return v;
	}
    }
    else { // load user-defined
	if ((v = load_user_file(fname, v, cip))!= E_SUCCEED) {
	    icb_free(cip);
	    return v;
	}
    }


    // now we should establish the calling argument
    // the original arguments are located at *(ip->st) and *(ip->st-1).
    // they must be copied onto cip->st, as a single list argument
    // the following 5 lines built up the calling list argument

    *(ip->st + 1) = *(ip->st);
    *(ip->st) = make_list(ip->st + 1);
    *(ip->st + 2) = con_nil;
    *(ip->st + 3) = make_list(ip->st - 1);
    ip->st += 3;

    /** We resized the Messenger to have enough room to fit the message, the copy_arg shall not fail... */
    cip->st = copy_arg(*(ip->st), ip->name_base, cip);

    // we should insert this new messenger into ready queue
    // and let the imago_creator to insert the parent imago back

    icb_put_signal(engine_queue, cip);

    // before return, we should make the parent ready

    ip->status = IMAGO_READY;

    return E_SUCCEED;
}

int imago_sys_dispatcher(ICBPtr ip) {
    ICBPtr cip = NULL;
    int v, t;
    char *cmd;
    /** Most of the time, system messengers are directed strait to the queen. */
    char *to = "queen";
    
    /** Things are different if it is the queen who dies.
	In that case, we have to dispatch 'syscall(kill)' 
	messengers to all worker. Unfortunatly, there is no
	garantee we find all workers in the log...
	Cloned workers may be known later on and in that case, 
	we would have missed them (cannot kill what you cannot see). */
    if( is_stationary(ip) ) {
	/** To everybody. */
	to = NULL;
	/** Only on command. Kill'em all ! ! ! */
	cmd = "kill";
    } else {
	/** First of all, make sure this imago is a worker,
	    We do not want to dispatch system messanger for nothing. */
	if( !is_worker(ip) )
	    return E_SUCCEED;

	/** The worker died/cloned on the queen machine... 
	    No need to tell anybody. */
	if( ip->queen.host_id == gethostid_cache() ) 
	    return E_SUCCEED;
	
	/** What command shoud we dispatch to this system messenger. */
	switch(ip->status) {
	case IMAGO_ERROR:   
	case IMAGO_DISPOSE: 	
	case IMAGO_TERMINATE:
	    cmd = "deceased";
	    break;
	    /** If this IP is ready to run, most likely it is a clone... */
	case IMAGO_CLONING:
	case IMAGO_READY: 
	    cmd = "clone";
	    break;
	default:
	    return E_ILLEGAL_STATUS;
	}
    }

    /** Then, grab a new ICB to put the system messenger into. */
    cip = icb_new();

    if( cip == NULL )
	return E_ICB_OUT;

    /** Copy the necessary informations. */
    strcpy(cip->name, ip->name);   
    cip->application = ip->application;
    cip->queen = ip->queen;
    cip->jumps = ip->jumps;
    get_imago_localhost( cip->citizenship.name, sizeof(cip->citizenship.name) );
  
    /** Load the messenger code. */
    if( (v = load_system_file("$sys_messenger", 0, cip)) != E_SUCCEED ) {
	warning("Unable to dispatch system messenger.\n" );
	icb_free(cip);
	return v;
    } 

    /** This little guy is not a regular messenger. */
    cip->type = SYS_MSGER;

    /** Now the tough part...
	We need to copy some arguments into the messenger's stack.
    */
    /** build up the list head. */
    *(cip->st) = make_list( cip->st + 1 );
    /** Destination. */
    if( to ) {
	v = search_index( to, cip, cip->st + 1 );
	*(cip->st + 1) = make_con( v + ASCII_SIZE );
    } else {
	*(cip->st + 1) = make_ref( cip->st + 1 );
    }
    /** Then make the second element in the list. */
    *(cip->st + 2) = make_list( cip->st + 3 );
    /** Add the command. */
    v = search_index( cmd, cip, cip->st + 3 );
    *(cip->st + 3) = make_con( v + ASCII_SIZE );
    /** End the list. */
    *(cip->st + 4) = con_nil;
    /** Put on top of the stack the list pointer to the list. */
    *(cip->st + 5) = make_ref( cip->st );
    /** Update the trail backtrack pointer. */
    deref( t, cip->st );
    bt_bbtrail( t, cip );
    /** update the stack top pointer. */
    cip->st += 5;
    /** */

    /** And we READY this little guy so he can grow big. */
    icb_put_signal(engine_queue, cip);

    return E_SUCCEED;
}

int name_table_expansion(ICBPtr cip, int* top){
    // this function is called when a new name (char string) is to be inserted
    // into the name_table but there is no enough free space. This algorithm
    // will grab a chunk of memory from stack and move the stack upward.

    int *oldp, *newp, *oldbase, *oldtop, distance, temp;

    distance = NAME_EXP_SIZE/sizeof(int);

    oldtop = oldp = top;
 
    newp = oldp + distance;
    oldbase = cip->stack_base;
    cip->stack_base += distance;
    cip->st += distance;
    cip->bb += distance;
    cip->b0 += distance;
    cip->cf += distance;
    cip->af += distance;
    cip->gl += distance;
      	
    // during moving the stack, we are facing different cases:
    // 1) address of the stack: adjusting wrt distance
    // 2) code address, such as CP, BP, no adjustment
    // 3) trail address, TT, no adjustment
    // 4) constants: atoms, integers and floating values, no adjustment
 
    while (oldp >= oldbase){    	
	if ((oldp - 2) > oldbase && is_fvl(*(oldp - 2))){
	    *newp-- = *oldp--;
	    *newp-- = *oldp--;
	    *newp-- = *oldp--;
	    continue;
	}
	else {
	    temp = *oldp--;
	    if (is_cst(temp) ||(addr(temp) < oldbase) 
		|| (addr(temp) > oldtop) ){
		*newp-- = temp;
		continue;
	    }
	    else{
		*newp-- = (int)(addr(temp) + distance) | tag(temp);
		continue;
	    }
	}
    }


    // now we should adjust trail

    newp = (int*) cip->tt + 1;
    while (newp < cip->block_ceiling){
	temp = *newp;
	if (is_var(temp)) *newp-- = (int) ((int*) temp + distance);
	else *newp-- = make_str(addr(temp) + distance);
    }

    return distance; // return expansion size (in word)
}

int search_index(char* pcon, ICBPtr child, int* top){

    // this function is called when a constant (string) must be
    // constructed in the child name_table. If we can find it in the child,
    // an index to the child name_table is returned, otherwise, the constant
    // must be copied to the child name_table.
    // The argument pcon give the constant to be searched.
    // The algorithm adopts sequential search (to be modified if low efficiency)
    // It is possible that the child name_table needs to be expanded,
    // if this happens, we shall pass top to the expansion algorithm.
    // This is because we must know the current stack top in order to make
    // the expansion, while child->st does not alwys point to the current
    // stack top (see copy_arg algorithm: which increases stack interleaved
    // with the call to search_index).

    // IMPORTANT: child->nl is another return value:
    // 0: no name table adjustment
    // otherwise: stack adjust distance

    // thus suppose ss is an old stack pointer, name is the string to be searched
    // and v an integer to hold retun value, and ip the name space, assume that
    // the current stack top is ip->st, the calling pattern is:
    //		v = search_index(name, ip, ip->st);
    //		ss += ip->nl;	// make an adjustment
    //		*ss = make_con(v + ASCII_SIZE);
    // however, if stack pointers are formed from ip->st, then they are update 
    // this is the safe way for successive calls to search_index 
  
    char *ccon, *cp, *pp;

    child->nl = 0;

    // pp: parent constant string
    pp = pcon;

    // cp: search starting pointer
    cp = ccon = child->name_base;
			
    while (cp < child->name_top){
	while (*pp && *pp == *cp){
	    pp++;
	    cp++;
	}
	if (!*pp && !*cp)// we found it
	    return (ccon - child->name_base); 
	// advance to the next string
	while (*cp++ && cp < child->name_top);
	ccon = cp;
	pp = pcon;
    }
    // if we come here, cp == child->name_top
    // name table overflow problem is solved by 
    // name_table_expansion(..)
 
    pp = pcon;

    if (cp + strlen(pp) >= (char*) child->stack_base){		
	child->nl = name_table_expansion(child, top);
    }

    while ((*cp++ = *pp++));	// copy the string
    child->name_top = cp;
    return (ccon - child->name_base);
}

int arg_size(int term, ICBPtr ip){
    int size = 0, i, v;
    int* stk = ip->st + 1;
    int* btk = stk;


    *stk++ = term;  // push(term);
    while(stk > btk ){ // !empty_stack()
	term = *(--stk); // term = pop();
 
	while (is_var(term) && term != *(int*) term)
	    term = *(int*)term;

	if (is_var(term)) size++;
	else if (is_int(term)) size++;
	else if (is_con(term)) size += 2;
	else if (is_list(term)){
	    size++;
	    *stk++ = *addr(term); // push(term.head);
	    *stk++ = *(addr(term) + 1); // push(term.tail);
	}
	else if (is_float(term)) size += 4;
	else { // a structure
	    size += 2;
	    derefstr( v, term );
	    for (i = 1; i <= arity(*addr(v)); i++)
		*stk++ = *(addr(v) + i); // push(term[i])
	}
    }
    debug("ARGUMENT SIZE = %d\n", size);
    return size;
}

int* copy_arg(int proot, char* name_base, ICBPtr child){
    // This algorithm is called whenever an argument needs to be copied.
    // proot give the argument-term, name_base gives the name space,
    // and child gives block space to hold the copy.
    // The algorithm will copy (compound) term proot from one imago
    // space to another imago's space. 
    // This algorithm uses broad-first linear scanning:
    // 1) copy proot to the current stack top (st + 1) or stack_base.
    // 2) scanning and copying: newly copy terms use sktop pointer, while
    // currently scanning term uses skbot pointer. We do not change the
    // any information in child's ICB.
    // 3) when these two pointers meet, the argument has been constructed.
    // 4) recopy the initial root to the current stack top
    // 5) return the stack top

    // Note: upon return, child->st keeps the stack top before copying
    // while returned pointer indicates the new stack top. Caller may
    // still access other arguments through child->st, however,
    // child->st must be eventually set to the returned value for
    // further execution.

    int *skbot;				// bottom of copy stack;
    int *sktop;				// top of copy stack
    int root, con;
    register int *ftt = (int *)(child->tt - 6); /** Two times the maximum size requested. */

    skbot = sktop = child->st + 1;	// get free stack bottom
 
    // add the first term to be scanned

    *sktop++ = proot;  // the argument from parent

    while (skbot < sktop){
	if( ftt <= sktop ) {
	    child->nl = IMAGO_EXPANSION;
	    return NULL;
	} 
	deref(root, skbot);
	if (is_var(root)){ // make the cell self ref
	    *skbot = (int)skbot; skbot++;
	    continue;
	}
	else if (is_int(root) || is_nil(root)){ // an integer
	    *skbot++ = root;
	    continue;
	}
	else if (is_fvl(root)){ // an already copied float value
	    skbot += 3;			// bypass
	    continue;
	}
	else if (is_con(root)){	
	    // a constant instance or a functor
	    // they can be handled the same way, 
	    // arity(root) == 0 if a constant
	    // search_index will resolve the naming spaces problem

	    if (fun_value(root) < ASCII_SIZE){
		*skbot++ = root;
		continue;
	    }
	    // a changing stack top
	    con = search_index(name_base + 
			       (fun_value(root) - ASCII_SIZE), child, sktop - 1); 
	    // adjust two scaning pointers: 
	    // child->nl : 0 or expansion_size
	    // if name_table_expansion() is called
	    skbot += child->nl;
	    sktop += child->nl;
	    *skbot++ = make_fun(con + ASCII_SIZE, arity(root));
	    continue;
	}
	else if (is_list(root)){	//  a list
	    *skbot++ = make_list((int)sktop);
	    *sktop++ = *addr(root);		// copy head
	    *sktop++ = *(addr(root) + 1); //copy tail
	    continue;
	}
	else{	// must be a str or float term
	    *skbot++ = make_str((int)sktop);
	    derefstr(root, root);
	    if (is_float(root)){
		*sktop++ = FVL_TAG;		// copy functor
		*sktop++ = *(addr(root) + 1);	// copy w1
		*sktop++ = *(addr(root) + 2);	// copy w2
		continue;
	    }
	    else{
		for(con = 0; con <= arity(*addr(root)); con++){ 
		    // copy parameters and functor
		    *sktop++ = *(addr(root) + con);
		}
		continue;
	    }
	}
    }

    // finally, we should copy the initial root onto the top of the stack
    // and then return the new stack top

    *sktop = *(child->st + 1);
    return sktop;
}

int load_user_file(char *fname, int ss, ICBPtr cip){
    // this file loader is called by imago_sys_loader and 
    // imago_dispatcher to load a user-defined local file

    FILE* fp;
    int magic, itype, security, ssize, psize, csize, snum, total;
    int* bp;

    if ((fp = fopen(fname, "r")) == NULL){
	return E_FILE_NOT_FOUND;
    }

    fscanf(fp, "%d%d%d%d%d%d", &magic, &itype, &security,
	   &ssize, &psize, &csize);

    // calculate and allocate a memory block
    // ssize (byte), psize*(term, entry), csize*(type, code)
    // where for each code, if type == 0, load directly
    // else relocate: code_base + offset

    if( itype == STATIONARY ){
	return E_ILLEGAL_CREATE;
    }

    // leave ~256 bytes for dynamic use
    snum = (ssize + NAME_EXP_SIZE)/sizeof(int); 
 
    total = snum + psize*2 + csize + ss;

    if( itype == WORKER )
	total +=  INIT_WORKER_STACK*STACK_UNIT;
    else
	total +=  INIT_MESSENGER_STACK*STACK_UNIT;

    // this is the point to insert the creating imago into a memory-waiting
    // queue. Note, release cip before return
    if ((bp = (int*) malloc(total*sizeof(int))) == NULL){
	return E_MEMORY_OUT;
    }

    // decide address of each segment

    cip->block_base = bp;
    cip->block_ceiling = bp + total;
    cip->proc_base = bp;
    cip->code_base = bp + psize*2;
    cip->name_base = (char*) (bp + psize*2 + csize);
    cip->name_top = (char*) cip->name_base + ssize;
    cip->stack_base = bp + snum + psize*2 + csize;

    // load code

    if (load_code(fp, cip) != E_SUCCEED)
	return E_UNKNOWN_OPCODE;
 	
    // finish loading
    fclose(fp);

    // make child ICB ready

    cip->type = itype;
    cip->status = IMAGO_READY;
    cip->expansions = 0;
    cip->collections = 0;
    cip->pp = cip->cp = cip->code_base;		// starting entry
    cip->bb = cip->b0 = cip->gl = cip->cf = cip->stack_base;
    cip->st =  cip->stack_base - 1; // leave one for set up argument
    cip->tt = (int**) (cip->block_ceiling - 1);
    cip->af = (int *)cip->tt;
    *cip->tt-- = (int*) make_str(cip->gl);

    return E_SUCCEED;
}

int load_system_file(char* mname, int ss, ICBPtr cip){
    // This function simulate cloner to copy code from a builtin messenger
    // to the new messenger pointed by cip. First we have to search the builtin
    // messenger from builtin_msger table, and assign bip to point to the
    // messenger's ICB. Then we allocate a block to cip wrt the information in bip,
    // note that bip points to a preloaded prototype without stack space. 
    // After that, we copy data and code from bip's block to cip's block and
    // make corresponding address adjustment.
    int v, distance, *tp1, *tp2;
    char *cp1, *cp2;
    ICBPtr bip = NULL;

    // search for the messenger
    for (v = 0; builtin_msger[v].fname != NULL; v++){
	if (strcmp(mname, builtin_msger[v].mname) == 0){
	    bip = builtin_msger[v].ip;
	    break;
	}
    }

    if (!bip){
	return E_UNKNOWN_IMAGO;  // messenger not found
    }

    tp2 = bip->block_base;
		
    v =  INIT_MESSENGER_STACK*STACK_UNIT + 
	(bip->block_ceiling - tp2) + ss;

    if ((tp1 = (int*) malloc(v*sizeof(int))) == NULL){
	return E_MEMORY_OUT;
    }

    // set up base pointers

    cip->block_base = tp1;
    cip->block_ceiling = tp1 + v;
    cip->proc_base = tp1;
    cip->code_base = tp1 + (bip->code_base - tp2);
    cip->name_base = (char*) tp1 + (bip->name_base - (char*) tp2); // it is a char*
    cip->name_top =  (char*) tp1 + (bip->name_top  - (char*) tp2);
    cip->stack_base = tp1 + (bip->block_ceiling - tp2);

    // tp1 is the new block base and tp2 is the old block base

    distance = tp1 - tp2;

    // 1) procedure table: move and adjust entries

    tp1 = cip->proc_base;
    tp2 = bip->proc_base;
    while (tp1 < cip->code_base){
	*tp1++ = *tp2;
	(int*) *tp1++ = (int*) *tp2++ + distance;
    }

    // 2) code: only adjust code entries

    tp1 = cip->code_base;			// get code stop point
    tp2 = bip->code_base;
    while (tp1 < (int*) cip->name_base){
	adjust_code(ctype(),
		    cdirect(),
		    cadjust(),
		    chash());
    }

    // 3) symbolic table: move only

    cp1 = cip->name_base;
    cp2 = bip->name_base;
    while (cp1 < cip->name_top) *cp1++ = *cp2++;

    // set up registers

    cip->type = MESSENGER;
    cip->status = IMAGO_READY;
    cip->expansions = 0;
    cip->collections = 0;
    cip->pp = cip->cp = cip->code_base;		// starting entry
    cip->bb = cip->b0 = cip->gl = cip->cf 
	= cip->af =  cip->stack_base;
    cip->st =  cip->stack_base - 1; // leave one for initial argument
    cip->tt = (int**) (cip->block_ceiling - 1);

    return E_SUCCEED;
}

int load_code(FILE *fp, ICBPtr cip){
    // load file pointed by fp to the memory block in cip
    // NOTE: file format differs from the block layout:
    // file: [system_data, name_table, proc_table, code]
    // block:[proc_table, code, name_table, stack...]
    // where system_data has been processed by the caller,
    // now we have to read name_table before proc_table and code.

    int *base, *tp, code, i;
    char  c, *cp;

    cp = cip->name_base;
    do {
	c = fgetc(fp);
    } while (c == '\n');
    ungetc(c, fp);
    while (cp < cip->name_top){
	c = fgetc(fp);
	if (c == '\n') *cp++ = '\0';
	else *cp++ = c;
    }

    // load procedure table

    base = tp = cip->proc_base;
    while (tp < cip->code_base){
	fscanf(fp, "%d%d", tp, tp+1);	// term, offset
	*(tp+1) = (int)(base + *(tp+1)); // relocate offset
	tp += 2;
    }

    // load byte code

    base = tp = cip->code_base;

    while (tp < (int*) cip->name_base){

	fscanf(fp, "%d", &code);
	*tp++ = code;
	switch (opcode_tab[code].type) {

	    caseop(I_STATIONARY):
		caseop(I_WORKER):
		caseop(I_MESSENGER):
		caseop(I_E):		// label
		fscanf(fp, "%d", tp);
	    *tp = (int)(base + *tp);
	    tp++;
	    break;

	    caseop(I_O):
		break;

	    caseop(I_N):		// number
 		caseop(I_C):		// constant (int/chars)
		caseop(I_F):		// functor
		caseop(I_B):		// builtin
  		caseop(I_I):		// integer
 		caseop(I_P):		// procedure
		fscanf(fp, "%d", tp++);
	    break;

	    caseop(I_R):		// floating value
		caseop(I_2N):
		caseop(I_NC):
		caseop(I_NF):		// integer functor
		caseop(I_NI):
		fscanf(fp, "%d%d", tp, (tp + 1));
	    tp += 2;
	    break;

	    caseop(I_3N):
 		caseop(I_NR):
		caseop(I_2NF):
		fscanf(fp, "%d%d%d", tp, (tp + 1), (tp + 2));
	    tp += 3;
	    break;

	    caseop(I_4N):
		fscanf(fp, "%d%d%d%d", tp, (tp + 1), (tp + 2), (tp + 3));
	    tp += 4;
	    break;

	    caseop(I_5N):
		fscanf(fp, "%d%d%d%d%d", tp, (tp + 1), (tp + 2),
		       (tp + 3), (tp + 4));
	    tp += 5;
	    break;

	    caseop(I_6N):
		fscanf(fp, "%d%d%d%d%d%d", tp, (tp + 1), (tp + 2),
		       (tp + 3), (tp + 4), (tp + 5));
	    tp += 6;
	    break;

	    caseop(I_7N):
		fscanf(fp, "%d%d%d%d%d%d%d", tp, (tp + 1), (tp + 2),
		       (tp + 3), (tp + 4), (tp + 5), (tp + 6));
	    tp += 7;
	    break;

	    caseop(I_8N):
		fscanf(fp, "%d%d%d%d%d%d%d%d", tp, (tp + 1), (tp + 2),
		       (tp + 3), (tp + 4), (tp + 5), (tp + 6), (tp + 7));
	    tp += 8;
	    break;

	    caseop(I_NE):
		fscanf(fp, "%d%d", tp, (tp + 1));
	    *(tp + 1) = (int)(base + *(tp + 1));
	    tp += 2;
	    break;

	    caseop(I_2NE):
		caseop(I_NFE):
		fscanf(fp, "%d%d%d", tp, (tp + 1), (tp + 2));
	    *(tp + 2) = (int)(base + *(tp + 2));
	    tp += 3;
	    break;

	    caseop(I_2NFE):
 		caseop(I_3NE):
		fscanf(fp, "%d%d%d%d", tp, (tp + 1), (tp + 2), (tp + 3));
	    *(tp + 3) = (int)(base + *(tp + 3));
	    tp += 4;
	    break;

	    caseop(I_3NFE):
		fscanf(fp, "%d%d%d%d%d", tp, (tp + 1), (tp + 2),
		       (tp + 3), (tp + 4));
	    *(tp + 4) = (int)(base + *(tp + 4));
	    tp += 5;
	    break;

	    caseop(I_4E):
		fscanf(fp, "%d%d%d%d", tp, (tp + 1), (tp + 2), (tp + 3));
	    *(tp) = (int)(base + *(tp));
	    *(tp + 1) = (int)(base + *(tp + 1));
	    *(tp + 2) = (int)(base + *(tp + 2));
	    *(tp + 3) = (int)(base + *(tp + 3));
	    tp += 4;
	    break;

	    caseop(I_N4E):
		fscanf(fp, "%d%d%d%d%d", tp, (tp + 1), (tp + 2), (tp + 3),
		       (tp + 4));
	    *(tp + 1) = (int)(base + *(tp + 1));
	    *(tp + 2) = (int)(base + *(tp + 2));
	    *(tp + 3) = (int)(base + *(tp + 3));
	    *(tp + 4) = (int)(base + *(tp + 4));
	    tp += 5;
	    break;

	    caseop(I_HTAB):		// hashtable
		fscanf(fp, "%d%d", tp, (tp + 1));
	    i = *tp; // number of pairs
	    *(tp + 1) = (int)(base + *(tp + 1)); // var branch
	    tp += 2;
	    while (i--){
		fscanf(fp, "%d%d", tp, (tp + 1));
		*(tp + 1) = (int)(base + *(tp + 1));
		tp += 2;
	    }
	    break;
	default:
	    return E_UNKNOWN_OPCODE;
	}
    }
    return E_SUCCEED;
}


int clone_block(ICBPtr ip, ICBPtr cip){
    // this function will be called by imago_cloner and 
    // load_sys_file for a builtin-messenger.
    // They both copy everything from an old block pointed by ip 
    // to new block pointed by cip.

    int v, distance, *tp1, *tp2;
    char *cp1, *cp2;

    // get distance of two blocks

    distance = cip->block_base - ip->block_base;		


    // The following similar to memory_expansion, except that the clone
    // always has a new block whereas memory expansion might inherit
    // the old block

    // adjust base pointers

    cip->block_ceiling = ip->block_ceiling + distance;
    cip->stack_base = ip->stack_base + distance;
    cip->code_base = ip->code_base + distance;
    cip->proc_base = ip->proc_base + distance;
    cip->name_base = ip->name_base + distance*sizeof(int); // it is a char*
    cip->name_top = ip->name_top + distance*sizeof(int);

    // adjust registers

    cip->pp = ip->pp + distance;
    cip->af = ip->af + distance;
    cip->cp = ip->cp + distance;
    cip->cf = ip->cf + distance;
    cip->st = ip->st + distance;
    cip->bb = ip->bb + distance;
    cip->b0 = ip->b0 + distance;
    cip->gl = ip->gl + distance;
    cip->tt = ip->tt + distance;

    printf("cloner: after register adjustment\n");
    fflush(stdout);

    /****************************************************************/
    /*                                                              */
    /*                     move each segments:                      */
    /* 1) procedure entry table must be adjusted                    */
    /* 2) code hast been moved but code entries should be adjusted  */
    /* 3) symbolic name table does not need adjustment              */
    /* 4) active stack must be scanned and pointers be adjusted     */
    /****************************************************************/

    // 1) procedure table: move and adjust entries

    tp1 = cip->proc_base;
    tp2 = ip->proc_base;
    while (tp1 < cip->code_base){
	*tp1++ = *tp2;
	(int*) *tp1++ = (int*) *tp2++ + distance;
    }


    // 2) code: only adjust code entries

    tp1 = cip->code_base;			// get code stop point
    tp2 = ip->code_base;
    while (tp1 < (int*) cip->name_base){
	adjust_code(ctype(),
		    cdirect(),
		    cadjust(),
		    chash());
    }

    // 3) symbolic table: move only

    cp1 = cip->name_base;
    cp2 = ip->name_base;
    while (cp1 < cip->name_top) *cp1++ = *cp2++;

    // 4) Linear scan the active stack
    // a) copy constants, a special case is that when
    //    a FVL_TAG is met, we have to copy successive three cells 
    //    (a floating value)
    // b) copy and adjust all tag_on_pointer terms  which include var,  
    //    str, list, and control pointers such as cp, cf, bb, etc.,
    //    in control frames

    tp1 = cip->stack_base;
    tp2 = ip->stack_base;

    while (tp1 <= cip->st){
	v = *tp2;
	if (is_cst(v)){
	    if (is_fvl(v)){ // bypass floating value
		*tp1++ = *tp2++;
		*tp1++ = *tp2++;
		*tp1++ = *tp2++;
	    }
	    else{
		*tp1++ = *tp2++;
	    }
	}
	else {
	    *tp1++ = (int) (addr(v) + distance) | tag(v);
	    tp2++;
	}
    }

    // now we should move and adjust trail

    tp1 = (int*) cip->tt;		// get trail top
    tp2 = (int*) ip->tt;	// old trail top

    while(tp1 < cip->block_ceiling - 1){ // an error occurs if not -1
	v = *(++tp2); // it seems laptop linux not doing boundary check
	if (is_var(v)) { // which changes the dynamic block chain pointer
	    *(++tp1) = (int)((int*) v + distance);
	}
	else {	// must be a generation line
	    v = (int) addr(v);
	    *(++tp1) = make_str((int*) v + distance);
	}
    }
 
    // TT's in CF chain and BB chain have already been adjusted
    return E_SUCCEED;
}

int release_msger(ICBPtr ip, char* name){
    ICBPtr mip;
    int v, t;

    que_lock(&ip->msq);

    for( mip = icb_first(&ip->msq); mip; mip = icb_first(&ip->msq) ) {
	// assign cloned(name) or moved(URL), or deceased to 
	// each "attached" messenger in msq.
	switch (ip->status){
	case IMAGO_ERROR:
	case IMAGO_DISPOSE:
	case IMAGO_TERMINATE:
	    v = search_index("deceased", mip, mip->st);
	    deref(t, mip->st);
	    *(int*) t = make_con(v + ASCII_SIZE);
	    bt_bbtrail(t, mip);
	    break;
	case IMAGO_MOVE_OUT:
	    v = search_index("moved", mip, mip->st);
	    *(mip->st + 1) = make_fun(v + ASCII_SIZE, 1);
	    v = search_index(name, mip, mip->st + 1);
	    *(mip->st + 2) = make_con(v + ASCII_SIZE);
	    deref(t, mip->st);
	    *(int*) t = make_str(mip->st + 1);
	    bt_bbtrail(t, mip);
	    mip->st += 2;
	    break;
	case IMAGO_CLONING:
	    v = search_index("cloned", mip, mip->st);
	    *(mip->st + 1) = make_fun(v + ASCII_SIZE, 1);
	    v = search_index(name, mip, mip->st + 1);
	    *(mip->st + 2) = make_con(v + ASCII_SIZE);
	    deref(t, mip->st);
	    *(int*) t = make_str(mip->st + 1);
	    bt_bbtrail(t, mip);
	    mip->st += 2;
	    break;
	default: 
	    /** Any other case, we have to make the messenger retry the attach built-in. */
	    debug("%s(%p): Invalid receiver status (%s->%d)\n",
		  __PRETTY_FUNCTION__, ip, ip->name, ip->status);
	    v = search_index("retry", mip, mip->st);
	    deref(t, mip->st);
	    *(int*) t = make_con(v + ASCII_SIZE);
	    bt_bbtrail(t, mip);
	    break;
	}
	mip->status = IMAGO_READY;
	icb_put_signal(engine_queue, mip);
    }
    que_unlock(&ip->msq);
    return E_SUCCEED;
}
