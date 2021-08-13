/** $RCSfile: engine.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: engine.c,v 1.18 2003/03/25 15:15:29 gautran Exp $ 
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
/*	 	engine.c                        */
/************************************************/


#include "opcode.h"
#include "engine.h"
#include "builtins.h"

/************************************************/
/* This function is called by the emulator      */
/* scheduler which passes the pointer of a      */
/* ready lvm_thread to the engine. The engine	*/
/* sequentially executes the code of the thread	*/
/* and returns to the scheduler in cases of:	*/
/* 1) the thread exit (finish)                  */
/* 2) the thread has been switched to GC        */
/* 3) the thread waits for a messenger          */
/* 4) the thread moves (clones)                 */
/* 5) and other possible cases                  */
/* As soon as the engine returns, the scheduler */
/* schedules the next ready thread to run.      */
/* In this implementation, we do not use time 	*/
/* slice to control the switch of threads. I	*/
/* think that GC points and some primitives	*/
/* (move, clone, etc) are proper places	for	*/
/* thread switching.                            */
/* Note: the loader(creator) of a thread must	*/
/* set up all the necessary data as well as	*/
/* initial register values in the thread control*/
/* block.                                       */
/* Upon context switch,  the execution status   */
/* of the current is saved in plt->status and   */
/* supplemental information is stored in        */
/* plt->nl, such as error code.                 */
/************************************************/

void engine(ICBPtr plt) {

    /*	Registers	*/

    register int*	pp = plt->pp;         // program pointer
    register int*	af = plt->af;         // active frame pointer
    register int*	cp = plt->cp;         // continuation program pointer
    register int*	cf = plt->cf;         // continuation frame pointer
    register int*	st = plt->st;         // stack pointer (successive put pointer)
    register int*	gp = 0;               // successive get/set pointer
    register int	ax = 0;               // register
    register int	bx = 0;               // register
    register int*	bb = plt->bb;         // backtrack frame pointer
    register int*	b0 = plt->b0;         // cut backtrack frame pointer
    register int*	gl = plt->gl;         // generation line
    register int**	tt = plt->tt;         // trail pointer
    register int	nl = plt->nl;         // nesting level
    register int	cx = 0;               // register
    register int	dx = 0;               // register
    register int	bt = BACKTRACK_LIMIT; // Backtrack count


    if (plt->status == IMAGO_BACKTRACK){
	plt->status = IMAGO_READY;
	backtrack();
    }

	  
    if( plt->status != IMAGO_READY )
	context_switch(IMAGO_ERROR, E_ILLEGAL_THREAD);

    /********************************************************************************/
    /*                                                                              */
    /* Stack frames:                                                                */
    /*                                                                              */
    /*       alloa: Activation frame (for normal clause - more than one goal)       */
    /*                                                                              */
    /*	---------                                                                   */
    /*	|  CF	|                                                                   */
    /*	|-------|                                                                   */
    /*	|  CP	|                                                                   */
    /*	|-------|                                                                   */
    /*	|  TT	|                                                                   */
    /*	|-------|                                                                   */
    /*	| vars	|                                                                   */
    /*	---------                                                                   */
    /*	try:   Choice frame  (for nondeternistic clause)                            */
    /*                                                                              */
    /*	---------                                                                   */
    /*	|  CF	|                                                                   */
    /*	|-------|                                                                   */
    /*	|  CP	|                                                                   */
    /*	|-------|                                                                   */
    /*	|  TT	|                                                                   */
    /*	|-------|                                                                   */
    /*	|  BB	|                                                                   */
    /*	|-------|                                                                   */
    /*	|  B0	|                                                                   */
    /*	|-------|                                                                   */
    /*	|  BP	|                                                                   */
    /*	|-------|                                                                   */
    /*	|  GL	|                                                                   */
    /*	|-------|                                                                   */
    /*	| vars	|                                                                   */
    /*	---------                                                                   */
    /*                                                                              */
    /*	allod:	Data frame (for chain clause and fact)                              */
    /*                                                                              */
    /*	---------                                                                   */
    /*	| vars	|                                                                   */
    /*	---------                                                                   */
    /*                                                                              */
    /* Note:                                                                        */
    /*                                                                              */
    /* 1) vars portion includes variables, flattened structures and call-arguments. */
    /*                                                                              */
    /* 2) each clause invocation must use af as the base to access each frame cell. */
    /*                                                                              */
    /* 3) before any frame allocation, st points to the last argument. Thus, its    */
    /*   content can be used to switch/hash to different entries (last-arg          */
    /*   dispatching).                                                              */
    /*                                                                              */
    /********************************************************************************/
    if (is_messenger(plt))
	debug("ENGINE: schedule %s's messenger\n", plt->name);
    else 
	debug("ENGINE: schedule imago %s\n", plt->name);


 CONTROL:
#ifdef TRACE
    trace_control(plt, *pp, 1);
#endif
    // trace_control(plt, *pp, 1);


    switch (*pp) {

	/************************************************/
	/*                 Instructions                 */
	/*          Control and memory allocation       */
	/************************************************/

	caseop(LVM_NOOP):

	    next(1);

	caseop(LVM_ALLOA):
	    // alloa n1 n2:  # of arguments and # of additional cells

	    // check if memory expansion is necessary
	    if( ((int*)tt - st) < (OD2 * GC_FACTOR) )
		context_switch(IMAGO_EXPANSION, OD1);
	   
	/**
	   if (is_messenger(plt)){
	   if ((int*)tt - st < INIT_MESSENGER_STACK/2){
	   context_switch(IMAGO_EXPANSION, OD1);
	   }
	   }
	   else if ((int*)tt - st < GC_LIMIT){
	   context_switch(IMAGO_EXPANSION, OD1);
	   }
	*/
	// check if GC is necessary

	if (cf > gl){ 	// cf does not belong to any generation

	    make_aframe();

	    if ((st - gl) > GC_LIMIT){ // need a new generation
		gl = st;
		new_generation(); // *tt-- = make_str(gl);
		//debug("Set Generation in ALLOA at : %x\n", st);
	    }
	    next(3);
	}
	else {
	    if (cf <= bb){ // cf belongs to a nondeterministic generation

		af = (int*) BTT;	// get root base;
	    }
	    else {
		af = (int*) CTT; // get generation search pointer;
	    }

	    while (tag(*af) != STR_TAG) af--; 	//search for gline

	    if ((st - addr(*af)) < GC_LIMIT){ // addr(*af) is gline
		// there is no enough garbage
		make_aframe();
		next(3);
	    }
	    else {
		context_switch(IMAGO_COLLECTION, OD1);
	    }
	}

	caseop(LVM_ALLOD):
	    // allod n1 n2: # of arguments and # of additional cells
	    // check if memory expansion is necessary
	    if( ((int*)tt - st) < (OD2 * GC_FACTOR) )
		context_switch(IMAGO_EXPANSION, OD1);

	/**
	   if (is_messenger(plt)){
	   if ((int*)tt - st < INIT_MESSENGER_STACK - 1){
	   context_switch(IMAGO_EXPANSION, OD1);
	   }
	   }
	   else if ((int*)tt - st < GC_LIMIT){
	   context_switch(IMAGO_EXPANSION, OD1);
	   }
	*/
	// check if GC is necessary

	if (cf > gl){ 	// cf does not belong to any generation

	    make_dframe();

	    if ((st - gl) > GC_LIMIT){ // need a new generation
		gl = st;
		new_generation(); // *tt-- = make_str(gl);
		//debug("Set Generation in ALLOD at : %x\n", st);
	    }
	    next(3);
	}
	else {
	    if (cf <= bb){ // cf belongs to a nondeterministic generation

		af = (int*) BTT;	// get root base;
	    }
	    else {
		af = (int*) CTT;	// get generation search pointer;
	    }

	    while (tag(*af) != STR_TAG) af--; 	//search for gline

	    if ((st - addr(*af)) < GC_LIMIT){ // addr(*af) is gline
		// there is no enough garbage
		make_dframe();
		next(3);
	    }
	    else {
		context_switch(IMAGO_COLLECTION, OD1);
	    }
	}


	caseop(LVM_CALL):
	    // call e: procedure call

	    cf = af;
	CCP = pp + 2;
	af = st;

#ifdef TRACE
	trace_call(plt, LAB1, st, TRACE_CALL);
#endif
	//	trace_call(plt, LAB1, st, TRACE_CALL);

	jump(LAB1);

	caseop(LVM_LASTCALL):
	    // lastcall e: last procedure call

	    cf = ACF;
	af = st;
	b0 = bb;

#ifdef TRACE
	trace_call(plt, LAB1, st, TRACE_LCALL);
#endif
	//	trace_call(plt, LAB1, st, TRACE_LCALL);

	jump(LAB1);

	caseop(LVM_CHAINCALL):
	    // chaincall e: chain call

	    af = st;
	b0 = bb;

#ifdef TRACE
	trace_call(plt, LAB1, st, TRACE_LCALL);
#endif

	jump(LAB1);

	caseop(LVM_PROCEED):
	    // proceed: proceed to parent

	    af = cf;

#ifdef TRACE
	trace_return(plt, TRACE_PROCEED);
#endif

	jump(ACP);

	caseop(LVM_RETURN):
	    // return: return to parent

	    af = ACF;

#ifdef TRACE
	trace_return(plt, TRACE_RETURN);
#endif

	jump(ACP);

	caseop(LVM_SHALLOWTRY): // temporarily implemented as same as "try"

	    caseop(LVM_TRY):
	    // try n e: number of args and next-entry

	    // check if memory expansion is necessary

	    /** Hummmmm... Don't know how to modify this to reduce the limit... */
	    if( ((int*)tt - st) < (CHOICE_SIZE * GC_FACTOR) )
		context_switch(IMAGO_EXPANSION, OD1);
	/**
	    if (is_messenger(plt)){
		if ((int*)tt - st < INIT_MESSENGER_STACK - 1){
		    context_switch(IMAGO_EXPANSION, OD1);
		}
	    }
	    else if ((int*)tt - st < GC_LIMIT){
		context_switch(IMAGO_EXPANSION, OD1);
	    }
	*/
	/** */
	// check if GC is necessary


	if (cf > gl){ 	// cf does not belong to any generation
	    make_cframe();		// allocates a choice frame
	    next(3);			// and mirrors arguments
	}
	else {
	    if (cf <= bb){ // cf belongs to a nondeterministic generation
		af = (int*) BTT;	// get root base;
	    }
	    else {
		af = (int*) CTT;	// get generation search pointer;
	    }

	    while (tag(*af) != STR_TAG) af--; //search for gline
	    //  make_cframe();
	    //next(3);


	    if ((st - addr(*af)) < GC_LIMIT){ // addr(*af) is gline
		// there is no enough garbage
		make_cframe();
		next(3);
	    }
	    else {
		context_switch(IMAGO_COLLECTION, OD1);
	    }

	}

	caseop(LVM_RETRY):
	    // retry n e:  # of args and next entry

	    detrail(BTT);
	cf = BCF;		// restore continuation frame
	CCP = BCP;		// restore continuation point
	BBP = LAB2;		// save alternative entry
	st = bb;		// restore stack top
	gl = st;		// reset generation line
	new_generation();  	// reset this nondeterministic generation

	// mirror arguments
	ax = OD1;		// get number of arguments
	af = st - (CHOICE_SIZE + ax);
	while (ax--) *(++st) = *(++af);
	af = st;

	next(3);

	caseop(LVM_TRUST):
	    // trust

	    detrail(BTT);
	cf = BCF;
	CCP = BCP;
	gl = BGL;		// restore previous generation line
	// trust deallocate this frame and reset st & at to the
	// last argument

	st = af = bb - CHOICE_SIZE;
	//	b0 = BB0; has been set in backtrack()
	bb = BBB;		// restore previous choice
	next(1);

	caseop(LVM_NECKCUT):
	    // neckcut
	    if (bb > b0){
		gl = BGL;		// restore old generation line

		// we do not consider filtering the trail in this version
		gp = (int*) tt;		// set ax the current trail top
		tt = BTT;		// set tt the previous trail top
		filter_trail();	// clear up useless roots this is important

		bb = b0;		// restore old choice
	    }
	next(1);

	caseop(LVM_DEEPLABEL):
	    // deeplabel n
	    *cell(OD1) = (int) b0;
	next(2);

	caseop(LVM_DEEPCUT):
	    // deepcut n
	    bb = (int*) *cell(OD1);
	next(2);

	caseop(LVM_BUILTIN):

	    /****************************************************************/
	    /*                                                              */
	    /* builtin b/k : b/k is the symbolic builtin name with arity    */
	    /* during compilation, it is translated to:                     */
	    /* LVM_BUILTIN n where n is the index to the btin_tab[].        */
	    /* The entry of the table gives (char* name, int arity, *f(),   */
	    /* int special, int legal). The legal indicates whether the     */
	    /* predicate is a legal one for the type of the running imago.  */
	    /* The special indicates whether the called	                    */
	    /* builtin requires trail-operation. If so, we have to push	    */
	    /* gl and tt to stack (in addition to the other arguments).	    */
	    /* The parameter order (btm to top) is arg1, ... argn, gl, tt.  */
	    /* when the builtin returns, it will push the new tt on the top */
	    /* of the stack.                                                */
	    /****************************************************************/
	   
	plt->tt = tt; // only these four might be used in builtins
	plt->st = st;
	plt->bb = bb;
	plt->gl = gl;		
	
	/** We need to check the authorization first. */
	if( is_stationary(plt) && (btin_tab[OD1].legal & LEGAL_STATIONARY) ) {
	    /** Valid. */
	} else if( is_worker(plt) && (btin_tab[OD1].legal & LEGAL_WORKER) ) {
	    /** Valid. */
	} else if( is_messenger(plt) && (btin_tab[OD1].legal & LEGAL_MESSENGER) ) {
	    /** Valid. */
	} else if( is_sysmsger(plt) && (btin_tab[OD1].legal & LEGAL_SYSMSGER) ) {
	    /** Valid. */
	} else {
	    /** Invalid. */
	    context_switch(IMAGO_ERROR, E_ILLEGAL_BUILTIN);
	}
	/** All good to go. */
	
	(btin_tab[OD1].bf)(plt);  // call builtin function
	if (plt->status == IMAGO_BACKTRACK){
	    plt->status = IMAGO_READY;
	    bt = 0; /** Force yield to release the ICB lock. */
	    backtrack();
	}
	if (plt->status != IMAGO_READY){
	    // at this point, plt->status and plt->nl have been set
	    // plt->bb, tt, gl and st are update,
	    // however, some other fields pointed by plt need to be saved

	    if (plt->status == IMAGO_WAITING) plt->pp = pp; // to be re-executed
	    else if (plt->status == IMAGO_EXPANSION) plt->pp = pp; // to be re-executed
	    else plt->pp = pp + 2; // the predicate has been finished

	    plt->af = af;
	    plt->cf = cf;
	    plt->cp = cp;
	    plt->b0 = b0;
	    return;
	}

	// if come here, we do not need context switch
	// and the builtin succeeds 

	tt = plt->tt; // only these two might be changed
	st = plt->st;
	next(2);


	caseop(LVM_FAIL):
	    backtrack();

	caseop(LVM_FINISH):
	debug("\n****************************************************\n");
	debug("        Engine: Imago %s has finished\n", plt->name);
	debug("        Total memory expansion = %d, stack size = %d\n",
	       plt->expansions, st - plt->stack_base);
	debug("        Total garbage collection = %d, trail size = %d\n",
	       plt->collections, plt->block_ceiling - (int*) tt);
	debug("****************************************************\n");
	context_switch(IMAGO_TERMINATE, E_SUCCEED);

	/************************************************/
	/*          Unification instructions            */
	/************************************************/

	/************************************************************************/
	/*                                                                      */
	/* Generally speaking, there are three sets of unification instructions:*/
	/*                                                                      */
	/* put: used by the caller to set up calling arguments.                 */
	/*      put instructions have to follow the argument order, i.e.        */
	/*      first put the 1st-arg, then the 2nd-arg, ...                    */
	/*                                                                      */
	/* get: used by the callee to initiate unification. There are           */
	/*      two sets of get operations: one set requires a variable         */
	/*      index and another set inherites the location of the the         */
	/*      previous get operation. Some get operations have the            */
	/*      branching ability wrt the argument type.                        */
	/*                                                                      */
	/* set: used by both caller and callee to construct new                 */
	/*      instances of structures. Like get, it has two sets of           */
	/*      operations: indexed and inherited. Set operations will          */
	/*      use destructive assignment.                                     */
	/*                                                                      */
	/************************************************************************/

	/****************************************/
	/*          put instructions            */
	/****************************************/


	caseop(LVM_PUTVOID):
	    // putvoid:
	    ++st;
	*st = make_ref(st);
	next(1);

	caseop(LVM_PUT2VOID):
	    // put2void:
	    ++st;
	*st = make_ref(st);
	++st;
	*st = make_ref(st);
	next(1);

	caseop(LVM_PUTVAR):
	    // putvar n:
	    gp = cell(OD1);
	*(++st) = *gp = make_ref(gp);
	next(2);

	caseop(LVM_PUT2VAR):
	    // put2var n1 n2:
	    gp = cell(OD1);
	*(++st) = *gp = make_ref(gp);
	gp = cell(OD2);
	*(++st) = *gp = make_ref(gp);
	next(3);

	caseop(LVM_PUT3VAR):
	    // put3var n1 n2 n3:
	    gp = cell(OD1);
	*(++st) = *gp = make_ref(gp);
	gp = cell(OD2);
	*(++st) = *gp = make_ref(gp);
	gp = cell(OD3);
	*(++st) = *gp = make_ref(gp);
	next(4);

	caseop(LVM_PUT4VAR):
	    // put4var n1 n2 n3 n4:
	    gp = cell(OD1);
	*(++st) = *gp = make_ref(gp);
	gp = cell(OD2);
	*(++st) = *gp = make_ref(gp);
	gp = cell(OD3);
	*(++st) = *gp = make_ref(gp);
	gp = cell(OD4);
	*(++st) = *gp = make_ref(gp);
	next(5);

	caseop(LVM_PUTRVAL):
	    // putrval n: put the var's value and reset the variable
	    gp = cell(OD1);
	*(++st) = *gp;
	*gp = make_ref(gp);
	next(2);

	caseop(LVM_PUTVAL):
	    // putval n:
	    *(++st) = *cell(OD1);
	next(2);

	caseop(LVM_PUT2VAL):
	    // put2val n1 n2:
	    *(++st) = *cell(OD1);
	*(++st) = *cell(OD2);
	next(3);

	caseop(LVM_PUT3VAL):
	    // put3val n1 n2 n3:
	    *(++st) = *cell(OD1);
	*(++st) = *cell(OD2);
	*(++st) = *cell(OD3);
	next(4);

	caseop(LVM_PUT4VAL):
	    // put4val n1 n2 n3 n4:
	    *(++st) = *cell(OD1);
	*(++st) = *cell(OD2);
	*(++st) = *cell(OD3);
	*(++st) = *cell(OD4);
	next(5);

	caseop(LVM_PUT5VAL):
	    // put5val n1 n2 n3 n4 n5:
	    *(++st) = *cell(OD1);
	*(++st) = *cell(OD2);
	*(++st) = *cell(OD3);
	*(++st) = *cell(OD4);
	*(++st) = *cell(OD5);
	next(6);

	caseop(LVM_PUT6VAL):
	    // put6val n1 n2 n3 n4 n5 n6:
	    *(++st) = *cell(OD1);
	*(++st) = *cell(OD2);
	*(++st) = *cell(OD3);
	*(++st) = *cell(OD4);
	*(++st) = *cell(OD5);
	*(++st) = *cell(OD6);
	next(7);

	caseop(LVM_PUT7VAL):
	    // put7val n1 n2 n3 n4 n5 n6 n7:
	    *(++st) = *cell(OD1);
	*(++st) = *cell(OD2);
	*(++st) = *cell(OD3);
	*(++st) = *cell(OD4);
	*(++st) = *cell(OD5);
	*(++st) = *cell(OD6);
	*(++st) = *cell(OD7);
	next(8);

	caseop(LVM_PUT8VAL):
	    // put8val n1 n2 n3 n4 n5 n6 n7 n8:
	    *(++st) = *cell(OD1);
	*(++st) = *cell(OD2);
	*(++st) = *cell(OD3);
	*(++st) = *cell(OD4);
	*(++st) = *cell(OD5);
	*(++st) = *cell(OD6);
	*(++st) = *cell(OD7);
	*(++st) = *cell(OD8);
	next(9);

	caseop(LVM_PUTCON):
	    // putcon c:
	    caseop(LVM_PUTINT):
	    // putint n
	    *(++st) = OD1;
	next(2);

	// NOTE: There is no putfloat instruction
	// a float term must be set first and use putval to
	// setup argument

	caseop(LVM_PUTLIST):
	    // putlist n:
	    *(++st) = make_list(cell(OD1));
	next(2);

	caseop(LVM_PUTSTR):
	    // putstr n: index n gives the position of a structure
	    *(++st) = make_str(cell(OD1));
	next(2);




	/****************************************/
	/*          set instructions            */
	/****************************************/

	caseop(LVM_SETVAR):
	    // setvar n:
	    gp = cell(OD1);
	*gp = make_ref(gp);
	next(2);

	caseop(LVM_CSETVAR):
	    // csetvar:
	    gp++;
	*gp = make_ref(gp);
	next(1);

	caseop(LVM_SETCON):
	    // setcon n c:
	    caseop(LVM_SETINT):
	    // setint n1 n2:
	    *(gp = cell(OD1)) = OD2;
	next(3);

	caseop(LVM_CSETCON):
	    // csetcon c:
	    caseop(LVM_CSETINT):
	    // csetint n:
	    *(++gp) = OD1;
	next(2);

	caseop(LVM_SETVAL):
	    // setval n1 n2:
	    *(gp = cell(OD1)) = (int) cell(OD2);
	next(3);

	caseop(LVM_CSETVAL):
	    // csetval n:
	    *(++gp) = (int) cell(OD1);
	next(2);

	caseop(LVM_SETFLOAT):
	    // setfloat n w1 w2: where w1 and w2 are a double float value
	    *(gp = cell(OD1)) = make_str(++st);
	*st = FVL_TAG;
	*(++st) = OD2;
	*(++st) = OD3;
	trail(gp);
	next(4);

	caseop(LVM_CSETFLOAT):
	    // csetfloat w1 w2:
	    *(++gp) = make_str(++st);
	*st = FVL_TAG;
	*(++st) = OD1;
	*(++st) = OD2;
	trail(gp);
	next(3);

	caseop(LVM_SETLIST):
	    // setlist n1 n2: where n2 list starting cell
	    *(gp = cell(OD1)) = make_list(cell(OD2));
	next(3);

	caseop(LVM_CSETLIST):
	    // csetlist n:
	    *(++gp) = make_list(cell(OD1));
	next(2);

	caseop(LVM_SETFUN):
	    // setfun n1 f/k:
	    *(gp = cell(OD1)) = OD2;
	next(3);

	caseop(LVM_CSETFUN):
	    // csetfun f/k:
	    *(++gp) = OD1;
	next(2);

	caseop(LVM_SETSTR):
	    // setstr n1 n2:
	    *(gp = cell(OD1)) = make_str(cell(OD2));
	next(3);

	caseop(LVM_CSETSTR):
	    // csetstr n:
	    *(++gp) = make_str(cell(OD1));
	next(2);

	/****************************************/
	/*	 Stream jump                    */
	/****************************************/

	caseop(LVM_JUMP):
	    // jump  e 

	    jump(LAB1);



	caseop(LVM_BRANCH):
	    // branch  e1 e2 e3 e4

	    if (nl < 4){
		pp = (int*)(*(pp + 1 + nl));
		goto CONTROL;
	    }
	    else {
		next(5);
	    }

	/****************************************/
	/*           get instructions           */
	/****************************************/

	caseop(LVM_GETCON):
	    // getcon n c:
	    caseop(LVM_GETINT):
	    // getint n1 n2:
	    ax = *(gp = cell(OD1));
    LGETCON:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LGETCON;
	    }
	    else{
		*(int*) ax = OD2;
		bbtrail(ax);
		next(3);
	    }
	}
	else if (ax == OD2){
	    next(3);
	}
	else {
	    backtrack();
	}

	caseop(LVM_CGETCON):
	    // cgetcon c:
	    caseop(LVM_CGETINT):
	    // cgetint n:
	    ax = *(++gp);
    LCGETCON:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LCGETCON;
	    }
	    else{
		*(int*) ax = OD1;
		bbtrail(ax);
		next(2);
	    }
	}
	else if (ax == OD1){
	    next(2);
	}
	else {
	    backtrack();
	}

	caseop(LVM_GETVAL):
	    // getval n1 n2:
	    // getval n1 n2:
	    ax = *(gp = cell(OD1));
	bx = *(cell(OD2));
	pp += 3;

	/****************************************************************/
	/* The following code implement general unification:            */
	/* a) prepare ax and bx                                         */
	/* b) prepare return address pp                                 */
	/* c) in case of nonunifiable, backtrack                        */
	/* d) in case of unifiable, go to control (pp)                  */
	/* e) this function will use st as the temprory stack.          */
	/****************************************************************/
    UNIFY:
	cx = (int) st;

	while(1){
	    // dereferencing ax and bx

	AXDEF:	  if (is_var(ax)){
	    if (ax != *(int*)(ax)){
		ax = *(int*) (ax);
		goto AXDEF;
	    }
	    else{
		*(int*) ax = bx;
		if (is_cst(bx)){
		    bbtrail(ax);
		}
		else {
		    trail(ax);
		}

		if ((int) st == cx){
		    goto CONTROL;	// succeed
		}
		else {
		    ax = *(st--);
		    bx = *(st--);
		    continue;
		}
	    }
	}

	BXDEF: 	  if (is_var(bx)){
	    if (bx != *(int*)(bx)){
		bx = *(int*) (bx);
		goto BXDEF;
	    }
	    else {
		*(int*) bx = ax;
		if (is_cst(ax)){
		    bbtrail(bx);
		}
		else {
		    trail(bx);
		}

		if ((int) st == cx){
		    goto CONTROL;	// succeed
		}
		else {
		    ax = *(st--);
		    bx = *(st--);
		    continue;
		}
	    }
	}

	if (ax != bx){
	    if (is_list(ax) && is_list(bx)){
		*(++st) = *(addr(bx) + 1);
		*(++st) = *(addr(ax) + 1);
		bx = *addr(bx);
		ax = *addr(ax);
		continue;
	    }
	    else if (is_float(ax) && is_float(bx)){
		if (*(addr(ax) + 1) != *(addr(bx) + 1) ||
		    *(addr(ax) + 2) != *(addr(bx) + 2)){
		    backtrack();
		}
		if ((int) st == cx){
		    goto CONTROL;	// succeed
		}
		else {
		    ax = *(st--);
		    bx = *(st--);
		    continue;
		}
	    }
	    else if (is_str(ax) && is_str(bx)){

		int* x = addr(ax);	// get str_term address
		int* y = addr(bx);
		ax = x[0];	// get functor
		bx = y[0];

		//			if (strcmp(&name_base[fun_value(ax)],
		//			   &name_base[fun_value(bx)])
		//				!= 0)

		// we assume that the same hash value applies
		// to the same functors

		if (ax != bx){
		    backtrack();
		}

		ax = arity(ax);

		while (ax > 1) { // push arguments
		    *(++st) = y[ax];
		    *(++st) = x[ax];
		    ax--;
		}
		bx = y[1];
		ax = x[1];
		continue;
	    }
	    else {
		backtrack();
	    }
	}
	else {
	    if ((int) st == cx){
		goto CONTROL;	// succeed
	    }
	    else {
		ax = *(st--);
		bx = *(st--);
		continue;
	    }
	}
	}

	caseop(LVM_CGETVAL):
	    // cgetval n:

	    ax = *(++gp);
	bx = *(cell(OD1));
	pp += 2;
	goto UNIFY;

	caseop(LVM_GETFLOAT):
	    // getfloat n w1 w2: where n: arg-index.  w1 w2: double float
	    ax = *(gp = cell(OD1));
    LGETFLOAT:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LGETFLOAT;
	    }
	    else{
		*(int*) ax = make_str(++st);
		*st = FVL_TAG;
		*(++st) = OD2;
		*(++st) = OD3;
		bbtrail(ax);
		next(4);
	    }
	}
	else if (is_float(ax)){
	    (int*) ax = addr(ax) + 1;
	    if (*(int*)ax == OD2 && *((int*)ax + 1) == OD3){
		next(4);
	    }
	    else{
		backtrack();
	    }
	}
	else{
	    backtrack();
	}

	caseop(LVM_CGETFLOAT):
	    // cgetfloat w1 w2:
	    ax = *(++gp);
    LCGETFLOAT:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LCGETFLOAT;
	    }
	    else{
		*(int*) ax = make_str(++st);
		*st = FVL_TAG;
		*(++st) = OD1;
		*(++st) = OD2;
		bbtrail(ax);
		next(3);
	    }
	}
	else if (is_float(ax)){
	    (int*) ax = addr(ax) + 1;
	    if (*(int*)ax == OD1 && *((int*)ax + 1) == OD2){
		next(3);
	    }
	    else{
		backtrack();
	    }
	}
	else{
	    backtrack();
	}


	caseop(LVM_GETLIST):
	    // getlist n1 n2 n3 e where n1: arg-index.
	    //                          n2: location to store the list
	    //                          n3: nesting level
	    //                          e: write stream entry
	    ax = *(gp = cell(OD1));
	bx = (int) cell(OD2);
    LGETLIST:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LGETLIST;
	    }
	    else{
		*(int*) ax = make_list(bx);
		trail(ax);
		gp = (int*) bx - 1; // goto cset instructions, so need -1
		nl = OD3;
		jump(LAB4);		// to write stream
	    }
	}
	else if (is_list(ax)){
	    *(int*) bx = *addr(ax);	// copy two arg's to [H|T]
	    *((int*)bx + 1) = *(addr(ax) + 1);
	    next(5);
	}
	else{
	    backtrack();
	}

	caseop(LVM_CGETLIST):
	    // cgetlist n1 n2 e:
	    ax = *(++gp);
	bx =(int) cell(OD1);
    LCGETLIST:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LCGETLIST;
	    }
	    else{
		*(int*) ax = make_list(bx);
		trail(ax);
		gp = (int*) bx - 1;
		nl = OD2;
		jump(LAB3);		// to write stream
	    }
	}
	else if (is_list(ax)){
	    *(int*) bx = *addr(ax);	// copy two arg's to [H|T]
	    *((int*)bx + 1) = *(addr(ax) + 1);
	    next(4);
	}
	else{
	    backtrack();
	}

	caseop(LVM_GETVLIST):
	    // getvlist n1 n2  where n1: arg-index.
	    //                       n2: location to store the list
	    ax = *(gp = cell(OD1));
	bx = (int) cell(OD2);
    LGETVLIST:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LGETVLIST;
	    }
	    else{
		*(int*) ax = make_list(bx);
		trail(ax);
		*(int*) bx = bx;		// init an [X|Y} list
		*((int*) bx + 1) = (int)((int*) bx + 1);
		next(3);
	    }
	}
	else if (is_list(ax)){
	    *(int*) bx = *addr(ax);	// copy two arg's to [H|T]
	    *((int*)bx + 1) = *(addr(ax) + 1);
	    next(3);
	}
	else{
	    backtrack();
	}

	caseop(LVM_CGETVLIST):
	    // cgetvlist n:
	    ax = *(++gp);
	bx =(int) cell(OD1);
    LCGETVLIST:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LCGETVLIST;
	    }
	    else{
		*(int*) ax = make_list(bx);
		trail(ax);
		*(int*) bx = bx;		// init an [X|Y} list
		*((int*) bx + 1) = (int)((int*) bx + 1);
		next(2);
	    }
	}
	else if (is_list(ax)){
	    *(int*) bx = *addr(ax);	// copy two arg's to [H|T]
	    *((int*)bx + 1) = *(addr(ax) + 1);
	    next(2);
	}
	else{
	    backtrack();
	}


	caseop(LVM_GETSTR):
	    // getstr n1 n2 n3 f/k e, where
	    // n1: arg-index
	    // n2: location of the structure
	    // n3: nesting level
	    // f/k: functor
	    // e: write-stream entry

	    ax = *(gp = cell(OD1));
	bx = (int) cell(OD2);
	dx = OD4;
    LGETSTR:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LGETSTR;
	    }
	    else{
		*(int*) ax = make_str(bx);
		trail(ax);
		gp = (int*) bx - 1;
		nl = OD3;
		jump(LAB5);		// to write stream
	    }
	}
	else if (is_str(ax)){
	    ax = (int) addr(ax);	// get the functor address
	    if (*(int*)ax != dx){  // functors do not match
		backtrack();
	    }
	    for (cx = 0; cx <= arity(dx); cx++){ // copy args
		*((int*)bx + cx) = *((int*)ax + cx);
	    }
	    next(6);
	}
	else{
	    backtrack();
	}

	caseop(LVM_CGETSTR):
	    // cgetstr n1 n2 f/k e
	    ax = *(++gp);
	bx = (int) cell(OD1);
	dx = OD3;
    LCGETSTR:
	if (is_var(ax)){
	    if (not_self_ref(ax)){
		ax = *(int*) (ax);
		goto LCGETSTR;
	    }
	    else{
		*(int*) ax = make_str(bx);
		trail(ax);
		gp = (int*) bx - 1;
		nl = OD2;
		jump(LAB4);		// to write stream
	    }
	}
	else if (is_str(ax)){
	    ax =(int) addr(ax);		// get the functor address
	    if (*(int*)ax != dx){  // functors do not match
		backtrack();
	    }
	    for (cx = 0; cx <= arity(dx); cx++){ // copy args
		*((int*)bx + cx) = *((int*)ax + cx);
	    }
	    next(5);
	}
	else{
	    backtrack();
	}

	/****************************************/
	/*       arithmetic instructions        */
	/****************************************/

	caseop(LVM_ADD):   	// *od3 = od1 + od2
	    ari_cpu(+);

	caseop(LVM_SUB):	// *od3 = od1 - od2
	    ari_cpu(-);

	caseop(LVM_MUL):	// *od3 = od1 * od2
	    ari_cpu(*);

	caseop(LVM_DIV):	// *od3 = od1 / od2
	    ari_cpu(/);

	caseop(LVM_MINUS): 	// *od2 = - od1
	    deref(ax, cell(OD1));
	if (is_int(ax)){
	    *cell(OD2) = make_int(-int_value(ax));
	    next(3);
	}
	else if (is_float(ax)){
	    *cell(OD2) = make_float((double)(-float_value(ax)), st);
	    adjust_st(3);
	    next(3);
	}
	context_switch(IMAGO_ERROR, E_UNKNOWN_TYPE);


	caseop(LVM_MOD):	// *od3 = od1 % od2
	    int_cpu(%);

	caseop(LVM_INC):	// *od2 = od1 + 1
	    int_one(+);

	caseop(LVM_DEC):	// *od2 = od1 - 1
	    int_one(-);

	caseop(LVM_IS):	// *od2 = od1
	    deref(bx, cell(OD2));
	if (is_var(bx)){
	    *(int*) bx = *cell(OD1);
	    bbtrail(bx);
	    next(3);
	}
	context_switch(IMAGO_ERROR, E_UNKNOWN_TYPE);

	caseop(LVM_AND):   	// *od3 = od1 & od2
	    int_cpu(&);

	caseop(LVM_OR):   	// *od3 = od1 | od2
	    int_cpu(|);

	caseop(LVM_EOR):   	// *od3 = od1 ^ od2
	    int_cpu(^);

	caseop(LVM_RSHIFT):  // *od3 = od1 >> od2
	    int_cpu(>>);

	caseop(LVM_LSHIFT):  // *od3 = od1 << od2
	    int_cpu(<<);


	/****************************************/
	/*       emu-builtin instructions       */
	/****************************************/

	/* term test		*/
	/* if true, goto next instruction, else jump to label */

	caseop(LVM_ISATOM):
	    // isatom n e: term is an atom
	    deref(ax, cell(OD1));
	if (is_con(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISATOMIC):
	    // isatomic n e: term is an atom or an int/float
	    deref(ax, cell(OD1));
	if (is_cst(ax) || is_float(ax)){ next(3);  }
	else jump(LAB2);

	caseop(LVM_ISFLOAT):
	    deref(ax, cell(OD1));
	if (is_float(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISINTEGER):
	    deref(ax, cell(OD1));
	if (is_int(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISNUMBER):
	    deref(ax, cell(OD1));
	if (is_int(ax) || is_float(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISNONVAR):
	    deref(ax, cell(OD1));
	if (!is_var(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISVAR):
	    deref(ax, cell(OD1));
	if (is_var(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISLIST):
	    deref(ax, cell(OD1));
	if (is_list(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISSTR):
	    deref(ax, cell(OD1));
	if (is_str(ax) && !is_float(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISCOMPOUND):
	    deref(ax, cell(OD1));
	if ((is_list(ax) || is_str(ax)) && !is_float(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_ISGROUND):
	    deref(ax, cell(OD1));
	// to be decided
	context_switch(IMAGO_ERROR, E_UNKNOWN_OPCODE);

	caseop(LVM_ISNIL):
	    // isnil n e:
	    deref(ax, cell(OD1));
	if (is_nil(ax)){ next(3); }
	else jump(LAB2);

	caseop(LVM_LASTNIL):
	    // lastnil e: based on the last argument *(st)
	    deref(ax, st);
	if (is_nil(ax)){ next(2); }
	else jump(LAB1);

	caseop(LVM_ISZERO):
	    // iszero n e:
	    deref(ax, cell(OD1));
	if (ax == MLVM_ZERO){ next(3);}
	else jump(LAB2);

	caseop(LVM_LASTZERO):
	    // lastzero e: based on the last argument *(st)
	    deref(ax, st);
	if (ax == MLVM_ZERO){ next(2); }
	else jump(LAB1);

	caseop(LVM_SWITCH):
	    // switch n ve ce le se: switch base on nth term
	    deref(ax, cell(OD1));
	jump((int*)*(pp + tag(ax) + 2));

	caseop(LVM_LASTSWITCH):
	    // lastswitch ve ce le se: switch base on last term
	    deref(ax, st);
	jump((int*)*(pp + tag(ax) + 1));

	caseop(LVM_HASHING):
	    // hashing n te: te is the hash-table entry
	    // which points to a table of the format:
	    // LVM_HASHINGTABLE, prime, var_branch, a1, e1, a2, e2, ...
	    // Note: the hash table uses linear probing for collision
	    // it is initialized with (0, var_branch) pairs. The key
	    // to the table is the con_value/fun_value without striping
	    // ASCII_SIZE, that is, we use the value directly against the
	    // prime to search for the procedure to be called. The condition
	    // stoping search is that a 0 entry (var_entry) is found. In
	    // this case the key in search is not included in the table.
	    // As the table size is larger than the number of keys, therefore
	    // there must be more than one 0 entry in the table.

	    deref(ax, cell(OD1));
	gp = LAB2;			// get hash table address
    DOHASH:
	dx = tag(ax);		// get the key tag
	cx = *(gp + 1);		// get prime
	if (dx == REF_TAG) jump((int*)*(gp + 2));
	if (dx == CON_TAG)
	    ax = con_value(ax);
	else if (dx == STR_TAG){
	    ax = *addr(ax); 	// get functor
	    ax = fun_value(ax);
	}
	else{
	    context_switch(IMAGO_ERROR, E_UNKNOWN_TYPE);
	}

	gp += 3;		// get starting address of the hash table

	// search for the key

	dx = ax%cx;		// get init entry index

	while (1){
	    pp = gp + dx*2;		// get entry address
	    if (!(*pp) || ax == *pp) break;	// no such entry or entry has been found
	    dx = (dx + 1) % cx;	// linear probing
	}

	jump((int*)*(pp + 1));

	caseop(LVM_LASTHASHING):
	    // lasthashing te
	    deref(ax, st);
	gp = LAB1;
	goto DOHASH;

	/****************************************/
	/* 		term comparison		*/
	/****************************************/

	caseop(LVM_IDENTICAL):
	    caseop(LVM_NONIDENTICAL):
	    caseop(LVM_PRECEDE):
	    caseop(LVM_PREIDENTICAL):
	    caseop(LVM_FOLLOW):
	    caseop(LVM_FLOIDENTICAL):
	    caseop(LVM_TERMCOMPARE):

	    /****************************************************************/
	    /* The following code implement generic term comparison:        */
	    /* a) var -> floating -> integer -> atom -> compound            */
	    /* b) vars are totally ordered (imple. dependent ? gc-influence */
	    /* c) numbers are ordered by arithmetic manner                  */
	    /* d) atoms are ordered in string comparing order               */
	    /* e) compound terms are ordered by arity, then by the functors */
	    /*    then by the arguments                                     */
	    /* f) this function will use st as the temprory stack.          */
	    /* g) prepare ax and bx as two comparing terms                  */
	    /* h) *pp is the request                                        */
	    /* i) return to next(4) if succ, to jump(LAB3) if fail          */
	    /*    except the last instruction: termcompare which set OD3 as */
	    /* MLVM_ZERO if term(ax) == term(bx)                            */
	    /* MLVM_ONE  if term(ax) @>  term(bx)                           */
	    /* MLVM_NONE if term(ax) @<  term(bx)                           */
	    /****************************************************************/

	    ax = *cell(OD1);
	bx = *cell(OD2);

	cx = (int) st;

	while(1){

	    // dereferencing ax and bx

	    while (is_var(ax) && not_self_ref(ax))
		ax = *(int*) (ax);

	    while (is_var(bx) && not_self_ref(bx))
		bx = *(int*) (bx);


	    if (is_var(ax)){
		if (is_var(bx)){
		    if (ax < bx) {
			dx = MLVM_NONE;
			break;
		    }
		    else if (ax > bx) {
			dx = MLVM_ONE;
			break;
		    }
		}
		else {
		    dx = MLVM_NONE;
		    break;
		}
	    }
	    else if (is_float(ax)){
		if (is_var(bx)){
		    dx = MLVM_ONE;
		    break;
		}
		else if (is_float(bx)){
		    double	da = float_value(ax);
		    double	db = float_value(bx);
		    if (da < db){
			dx = MLVM_NONE;
			break;
		    }
		    else if (da > db){
			dx = MLVM_ONE;
			break;
		    }
		}
		else{
		    dx = MLVM_NONE;
		    break;
		}
	    }
	    else if (is_int(ax)){
		if (is_var(bx) || is_float(bx)){
		    dx = MLVM_ONE;
		    break;
		}
		else if (is_int(bx)){
		    if (ax < bx){
			dx = MLVM_NONE;
			break;
		    }
		    else if (ax > bx) {
			dx = MLVM_ONE;
			break;
		    }
		}
		else{
		    dx = MLVM_NONE;
		    break;
		}
	    }
	    else if (is_con(ax)){
		if (is_var(bx) || is_float(bx) || is_int(bx)){
		    dx = MLVM_ONE;
		    break;
		}
		else if (is_con(bx)){
		    if (ax != bx){
			ax = con_value(ax);
			bx = con_value(bx);
			if (ax <= ASCII_SIZE || bx <= ASCII_SIZE){
			    if (ax > bx) dx = MLVM_ONE;
			    else dx = MLVM_NONE;
			    break;
			}
			ax -= ASCII_SIZE; // get name array index
			bx -= ASCII_SIZE;

			dx = strcmp(plt->name_base + ax,
				    plt->name_base + bx);
			if (dx > 0){
			    dx = MLVM_ONE;
			    break;
			}
			else if (dx < 0){
			    dx = MLVM_NONE;
			    break;
			}
		    }
		}
		else {
		    dx = MLVM_NONE;
		    break;
		}
	    }
	    else if (is_list(ax)){
		if (is_float(bx) || tag(bx) < LIS_TAG) {
		    dx = MLVM_ONE;
		    break;
		}
		else if (is_list(bx)){
		    *(++st) = (int)(addr(bx) + 1);
		    *(++st) = (int)(addr(ax) + 1);
		    *(++st) = (int) addr(bx);
		    *(++st) = (int) addr(ax);
		}
		else {
		    dx = MLVM_NONE;
		    break;
		}
	    }
	    else { 		// ax must be a structure
		if (is_float(bx) || tag(bx) < STR_TAG) {
		    dx = MLVM_ONE;
		    break;
		}
		else { 	// bx is also a structure

		    int* x = addr(ax);	// get str_term address
		    int* y = addr(bx);
		    ax = x[0];	// get functor
		    bx = y[0];

		    ax = fun_value(ax);
		    bx = fun_value(bx);
		    if (ax <= ASCII_SIZE || bx <= ASCII_SIZE){
			if (ax > bx) dx = MLVM_ONE;
			else dx = MLVM_NONE;
			break;
		    }
		    ax -= ASCII_SIZE; // get name array index
		    bx -= ASCII_SIZE;

		    dx = strcmp(plt->name_base + ax,
				plt->name_base + bx);
		    if (dx > 0) {
			dx = MLVM_ONE;
			break;
		    }
		    else if (dx < 0) {
			dx = MLVM_NONE;
			break;
		    }

		    ax = arity(*x);

		    while (ax) { // push arguments
			*(++st) = y[ax];
			*(++st) = x[ax];
			ax--;
		    }
		}
	    }

	    if ((int)st == cx){
		dx = MLVM_ZERO;
		break;
	    }
	    else {
		ax = *(st--);
		bx = *(st--);
	    }
	}

	switch (*pp){
	    caseop(LVM_IDENTICAL):
		// identical n1 n2 e (X == Y)
		if (dx == MLVM_ZERO){ next(4);  }
		else jump(LAB3);

	    caseop(LVM_NONIDENTICAL):
		// X \== Y
		if (dx != MLVM_ZERO){ next(4);  }
		else jump(LAB3);

	    caseop(LVM_PRECEDE):
		// X &< Y
		if (dx == MLVM_NONE){ next(4);  }
		else jump(LAB3);

	    caseop(LVM_PREIDENTICAL):
		// X @=< Y
		if (dx <= MLVM_ZERO){ next(4);  }
		else jump(LAB3);

	    caseop(LVM_FOLLOW):
		// X @> Y
		if (dx == MLVM_ONE){ next(4);  }
		else jump(LAB3);

	    caseop(LVM_FLOIDENTICAL):
		// X @>= Y
		if (dx >= MLVM_ZERO){ next(4);  }
		else jump(LAB3);

	    caseop(LVM_TERMCOMPARE):
		// X ?== Y (OD3 <= result of comparison)
		*cell(OD3) = dx;
	    next(4);
	}

	/****************************************/
	/*        arithmetic comparison         */
	/* two-way branch instructions except   */
	/* the last one: aricompare which set   */
	/* dx as the result.                    */
	/****************************************/

	caseop(LVM_IFEQ):
	    // equal n1 n2 e (X =:= Y)
	    ari_cmp(==);

	caseop(LVM_IFNE):
	    // X =\= Y
	    ari_cmp(!=);

	caseop(LVM_IFLT):
	    // X < Y
	    ari_cmp(<);

	caseop(LVM_IFLE):
	    // X <= Y
	    ari_cmp(<=);

	caseop(LVM_IFGT):
	    // X > Y
	    ari_cmp(>);

	caseop(LVM_IFGE):
	    // X >= Y
	    ari_cmp(>=);

	caseop(LVM_ARICMP):
	    // we should use the third operand to store comparison result,
	    // then ifeq0, ifne0, ... can be used to jump

	    // X =?= Y (dx <- result of comparison)
	    deref(ax, cell(OD1));
	deref(bx, cell(OD2));
	if (is_int(ax)){
	    if (is_int(bx)){
		if (ax > bx){
		    *cell(OD3) = MLVM_ONE;
		}
		else if (ax < bx) {
		    *cell(OD3) = MLVM_NONE;
		}
		else {
		    *cell(OD3) = MLVM_ZERO;
		}
		next(4);
	    }
	    else if (is_float(bx)){
		double d1 = (double) int_value(ax);
		double d2 = float_value(bx);
		if (d1 > d2 ){
		    *cell(OD3) = MLVM_ONE;
		}
		else if (d1 < d2) {
		    *cell(OD3) = MLVM_NONE;
		}
		else {
		    *cell(OD3) = MLVM_ZERO;
		}
		next(4);
	    }
	}
	else if (is_float(ax)){
	    if (is_int(bx)){
		double d1 = float_value(ax);
		double d2 = (double) int_value(bx);
		if (d1 > d2 ){
		    *cell(OD3) = MLVM_ONE;
		}
		else if (d1 < d2) {
		    *cell(OD3) = MLVM_NONE;
		}
		else {
		    *cell(OD3) = MLVM_ZERO;
		}
		next(4);
	    }
	    else if (is_float(bx)){
		double d1 = float_value(ax);
		double d2 = float_value(bx);
		if (d1 > d2 ){
		    *cell(OD3) = MLVM_ONE;
		}
		else if (d1 < d2) {
		    *cell(OD3) = MLVM_NONE;
		}
		else {
		    *cell(OD3) = MLVM_ZERO;
		}
		next(4);
	    }
	}
	context_switch(IMAGO_ERROR, E_UNKNOWN_TYPE);

	/****************************************/
	/*           jump instructions          */
	/* Note:                                */
	/* those instructions not only used for */
	/* simple comparison of a value, but    */
	/* also used after term_comparison,     */
	/* arithmetic_comparison, or tryunify.  */
	/*                                      */
	/****************************************/

	caseop(LVM_IFEQ0):
	    // ifeq0 n e: if *cell(OD1) == MLVM_ZERO then next else e
	    deref(ax, cell(OD1));
	if (ax == MLVM_ZERO){ next(3); }
	else jump(LAB2);

	caseop(LVM_IFNE0):
	    // ifne0 n e:
	    deref(ax, cell(OD1));
	if (ax != MLVM_ZERO){ next(3); }
	else jump(LAB2);

	caseop(LVM_IFLT0):
	    // iflt0 n e:
	    deref(ax, cell(OD1));
	if (ax < MLVM_ZERO){ next(3);   }
	else jump(LAB2);

	caseop(LVM_IFLE0):
	    // ifle0 n e:
	    deref(ax, cell(OD1));
	if (ax <= MLVM_ZERO){ next(3);  }
	else jump(LAB2);

	caseop(LVM_IFGT0):
	    // ifgt0 n e:
	    deref(ax, cell(OD1));
	if (ax > MLVM_ZERO){ next(3);  }
	else jump(LAB2);

	caseop(LVM_IFGE0):
	    // ifge0 n e:
	    deref(ax, cell(OD1));
	if (ax >= MLVM_ZERO){ next(3);  }
	else jump(LAB2);


	/****************************************/
	/*             unifiable                */
	/****************************************/

	caseop(LVM_UNIFIABLE):
	    //  unifiable n1 n2 e : (X = Y)

	    caseop(LVM_NONUNIFIABLE):
	    //  nonunifiable n1 n2 e: (X \= Y)

	    caseop(LVM_TRYUNIFIABLE):
	    // tryunifiable n1 n2 n3
	    // tryunifiable only check if X and Y are unifiable
	    // this instruction should also use cell(OD3) for storing result
	    // then ifeq0, ifne0, ... can be used for jump
	    // no matter succeed or fail, we have to clear up all bindings
	    // so it seems no unification has ever occurred


	    /****************************************************************/
	    /* The following code implement general unification:            */
	    /* a) prepare ax and bx                                         */
	    /* b) *pp holds opcode                                          */
	    /* c) in case of nonunifiable, dx <- MLVM_NONE                  */
	    /* d) in case of unifiable, dx <- MLVM_ZERO                     */
	    /* e) this code will use cx = st as the temprory stack.         */
	    /* f) this code will use gp, and thus cset/cget type inst can   */
	    /*    not be used after this                                    */
	    /****************************************************************/

	    ax = *cell(OD1);
	bx = *cell(OD2);
	cx = (int) st;
	dx = MLVM_ZERO;
	gp = (int*) tt;
	while(1){
	    // dereferencing ax and bx

	UAXDEF:	  if (is_var(ax)){
	    if (ax != *(int*)(ax)){
		ax = *(int*) (ax);
		goto UAXDEF;
	    }
	    else{
		*(int*) ax = bx;
		if (is_cst(bx)){
		    bbtrail(ax);
		}
		else {
		    trail(ax);
		}

		if ((int) st == cx){
		    break;	// succeed
		}
		else {
		    ax = *(st--);
		    bx = *(st--);
		    continue;
		}
	    }
	}

	UBXDEF:   if (is_var(bx)){
	    if (bx != *(int*)(bx)){
		bx = *(int*) (bx);
		goto UBXDEF;
	    }
	    else {
		*(int*) bx = ax;
		if (is_cst(ax)){
		    bbtrail(bx);
		}
		else {
		    trail(bx);
		}

		if ((int) st == cx){
		    break;	// succeed
		}
		else {
		    ax = *(st--);
		    bx = *(st--);
		    continue;
		}
	    }
	}

	if (ax != bx){
	    if (is_list(ax) && is_list(bx)){
		*(++st) = *(addr(bx) + 1);
		*(++st) = *(addr(ax) + 1);
		bx = *addr(bx);
		ax = *addr(ax);
		continue;
	    }
	    else if (is_float(ax) && is_float(bx)){
		if (*(addr(ax) + 1) != *(addr(bx) + 1) ||
		    *(addr(ax) + 2) != *(addr(bx) + 2)){
		    dx = MLVM_NONE;
		    st = (int*) cx;
		    break;
		}
		if ((int) st == cx){
		    break;	// succeed
		}
		else {
		    ax = *(st--);
		    bx = *(st--);
		    continue;
		}
	    }
	    else if (is_str(ax) && is_str(bx)){

		int* x = addr(ax);	// get str_term address
		int* y = addr(bx);
		ax = x[0];	// get functor
		bx = y[0];

		if (ax != bx) {
		    dx = MLVM_NONE;
		    st = (int*) cx;
		    break;
		}

		ax = arity(ax);

		while (ax > 1) { // push arguments
		    *(++st) = y[ax];
		    *(++st) = x[ax];
		    ax--;
		}
		bx = y[1];
		ax = x[1];
		continue;
	    }
	    else {
		dx = MLVM_NONE;
		st = (int*) cx;
		break;
	    }
	}
	else {
	    if ((int) st == cx){
		break;	// succeed
	    }
	    else {
		ax = *(st--);
		bx = *(st--);
		continue;
	    }
	}
	}


	// at this point, dx could be either MLVM_ZERO (succeed)
	// or MLVM_NONE (failed). However, all bindings in the
	// above process were trailed through gp. Thus different
	// instructions require different clean up activities

	ax = *pp;
	if (ax == LVM_UNIFIABLE){
	    // X = Y unifiable n1 n2 e

	    if (dx == MLVM_ZERO){
		// if succeed, we have to check trails which
		// were set during unification. Some of them
		// are below bb and some of them are above bb.
		// all bindings above bb will be deleted

		filter_trail();
		next(4);
	    }
	    else {
		// clear up all bindings in case of failed
		clear_trail();
		jump(LAB3);
	    }
	}
	else if (ax == LVM_NONUNIFIABLE){
	    // X \= Y nonunifiable n1 n2 e
	    // when X and Y are unifiable, then they are unified

	    if (dx == MLVM_NONE){
		// clear up all bindings when failed
		clear_trail();
		next(4);
	    }
	    else {
		filter_trail();
		jump(LAB3);
	    }
	}
	else { // ax == LVM_TRYUNIFIABLE
	    // tryunifiable only check if X and Y are unifiable
	    // no matter succeed or fail, we have to clear up all bindings
	    // so it seems no unification has ever occurred
	    // when returned, dx holds the result

	    clear_trail();
	    *cell(OD3) = dx;
	    next(4);
	}

	caseop(LVM_STATIONARY):
	    // simulate an activation frame

	    af = st = st + 2; // three cells
	ACF = cf;
	ATT = tt;

	// assume two parameters (to be modified)
	// first unbound, the second from parent
	++st;
	*st = make_ref(st);
	++st;
	*st = make_ref(st);

	// simulate a call

	cf = af;
	CCP = pp;

	/** Make sure the queen we are starting has an entry procedure defined. */
	if( LAB1 == pp ) {
	    warning( "Stationnary Imago '%s' does not have entry procedure\n", plt->name );
	    context_switch( IMAGO_ERROR, E_ILLEGAL_ENTRY );
	} else {
	    notice("Stationary imago '%s' started\n", plt->name);
	}
	// af = st;

#ifdef TRACE
	trace_call(plt, LAB1, st, TRACE_CALL);
#endif

	jump(LAB1);

	caseop(LVM_WORKER):
	    caseop(LVM_MESSENGER):
	    // simulate an activation frame
	    // the stack already hold initial parameter

	    af = st = st + 3; // three cells
	ACF = cf;
	ATT = tt;

	// assume two parameters (to be modified)
	// first unbound, the second from parent
	++st;
	*st = make_ref(st);
	++st;
	//	*st = make_ref(st);

	*st = *(af - 3); // the actual parameter

	// simulate a call

	cf = af;
	af = st;
	CCP = pp;

	/** Make sure the worker or messenger we are starting has an entry procedure defined. */
	if( LAB1 == pp ) {
	    warning( "%s Imago '%s' does not have entry procedure\n",
		     (*pp == LVM_WORKER)? "Worker":"Messenger", plt->name );
	    context_switch( IMAGO_ERROR, E_ILLEGAL_ENTRY );
	} else {
	    notice("%s imago '%s' started\n",
		   (*pp == LVM_WORKER)? "Worker":"Messenger", plt->name );
	}


#ifdef TRACE
	trace_call(plt, LAB1, st, TRACE_CALL);
#endif

	jump(LAB1);

    default:
	context_switch(IMAGO_ERROR, E_UNKNOWN_OPCODE);

    }


 CONTEXIMAGO_SWITCH:	// all context switches come to here

    // plt->status and plt->nl have been set

    plt->pp = pp;	// program pointer
    plt->af = af;	// gc line pointer
    plt->cp = cp;	// continuation program pointer
    plt->cf = cf;	// continuation frame pointer
    plt->st = st;	// stack pointer (successive put pointer)
    plt->bb = bb;	// backtrack frame pointer
    plt->b0 = b0;	// cut backtrack frame pointer
    plt->gl = gl;	// generation line
    plt->tt = tt;	// trail pointer
    return;
}


