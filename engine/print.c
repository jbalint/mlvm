/** $RCSfile: print.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: print.c,v 1.9 2003/03/21 13:12:03 gautran Exp $ 
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
#include "opcode.h"
#include "engine.h"
#include "builtins.h"


void print_term( int, char*, int );

void print_var( int fd, int t ) {
    dprintf( fd, "_%d", t );// _XXXX old: t % MAX_VAR
}

void print_int( int fd, int t ) {
    dprintf( fd, "%d", int_value(t) );
}

void print_constant( int fd, char* nms, int t ) {
    int v = con_value(t);

    if (v < ASCII_SIZE)
	dprintf( fd, "%c", v );
    else
	dprintf( fd, "%s", nms + v - ASCII_SIZE );
}

void print_functor( int fd, char* nms, int t ) {
    int v = fun_value(t);

    if (v <= ASCII_SIZE)
	dprintf( fd, "%c", v);
    else
	dprintf( fd, "%s", nms + v - ASCII_SIZE);
    
}

void print_float( int fd, int t){
    double f = float_value(t);
    dprintf( fd, "%g", f);
}

void print_structure( int fd, char* nms, int t){
    int i, num;
    int* p = addr(t);

    print_functor( fd, nms, *p );
    num = arity(*p);
    dprintf( fd, "(");
    for (i = 1; i <= num; i++){
	print_term(fd, nms, p[i]);
	if (i < num) dprintf( fd, ",");
    }
    dprintf( fd, ")");
}

void print_list( int fd, char* nms, int t){
    dprintf( fd, "[");
    while(1){
	int* p = addr(t);	// get list pointer
	print_term(fd, nms, p[0]);	// print head
	t = p[1];		// check possible tail flags:

	while (is_var(t) && t != *(int*)(t))
	    t = *(int*) (t);

	if (is_list(t)){	// list, var, or nil
	    dprintf( fd, ",");
	    continue;
	}
	else if (is_nil(t)){
	    break;
	}
	else if (is_var(t)){
	    dprintf( fd, "|_%d",  t); // t % MAX_VAR;
	    break;
	}
	else {
	    dprintf( fd, "TRACER:: list format error");
	    break;
	}
    }
    dprintf( fd, "]");
    fsync(fd);
}

void print_term( int fd, char* nms, int t){

    // dereference

    while (is_var(t) && t != *(int*)(t))
	t = *(int*) (t);

    switch (tag(t)){

    case REF_TAG:	print_var( fd, t);
	break;

    case CST_TAG:	if (is_int(t)) print_int( fd, t);
    else print_constant( fd, nms, t);
	break;

    case LIS_TAG:	print_list( fd, nms, t);
	break;

    case STR_TAG:
	if (is_float(t)) print_float( fd, t);
	else print_structure( fd, nms, t);
	break;
    }
}

void print_goal( int fd, char* nms, int* pp, int* st){
    int pf, num, i;
    pf = *(pp - 1);
    print_functor(fd, nms, pf);
    num = arity(pf);
    st -= num;
    dprintf( fd, "(");
    for (i = 1; i <= num; i++){
	print_term(fd, nms, *(++st));
	if (i < num) dprintf( fd, ", ");
    }
    dprintf( fd, ")\n");
}

void print_proc_table( int fd, char* nms, int* pp, int *sp){
    int* base = pp;
    while (pp < sp){
	print_functor(fd,  nms, *pp);
	dprintf( fd, "/%d ", arity(*pp++));
	dprintf( fd, "E%d\n", ((int*)*pp++ - base));
    }
}

void print_code( int fd, char* nms, int* pp, int* st){
    int* base = pp;
    int op, i;
    double d;
    int* pd = (int*) &d;

    dprintf( fd, "Print_code: nms = %d, pp = %d, st = %d\n",
	     (int)nms, (int) pp, (int) st);

    while (pp < st){

	op = *pp++;
	//		dprintf( fd, "OP = %d\n", op);

	dprintf( fd, "%s ", opcode_tab[op].name);

	switch (opcode_tab[op].type) {

	    caseop(I_STATIONARY): // op e func
		caseop(I_WORKER):
		caseop(I_MESSENGER):
		dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_O):
		break;

	    caseop(I_N):		// number
		dprintf( fd, "%d ",  *pp++);
	    break;

	    caseop(I_E):		// label
		dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_C):		// constant (int/chars)
		print_constant(fd, nms,*pp++);
	    break;

	    caseop(I_F):		// functor
		print_functor(fd, nms, *pp);
	    dprintf( fd, "/%d", arity(*pp++));
	    break;

	    caseop(I_B):		// builtin
		dprintf( fd, "Index = %d\n", *pp);
	    dprintf( fd, "%s", btin_tab[*pp++].name);
	    break;

	    caseop(I_I):		// integer
		print_int( fd, *pp++);
	    break;

	    caseop(I_P):		// procedure
		print_functor(fd, nms, *pp);
	    dprintf( fd, "/%d ", arity(*pp++));
	    dprintf( fd, "AT %d ", pp - base );
	    break;

	    caseop(I_R):		// floating value
		*pd = *pp++;
	    *(pd+1) = *pp++;
	    dprintf( fd, "%f ", d);
	    break;

	    caseop(I_2N):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    break;

	    caseop(I_3N):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    break;

	    caseop(I_4N):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    break;

	    caseop(I_5N):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    break;

	    caseop(I_6N):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    break;

	    caseop(I_7N):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    break;

	    caseop(I_8N):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    break;


	    caseop(I_NE):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_NC):
		dprintf( fd, "%d ",  *pp++);
	    print_constant(fd, nms,*pp++);
	    break;

	    caseop(I_NF):		// integer functor
		dprintf( fd, "%d ",  *pp++);
	    print_functor(fd, nms, *pp);
	    dprintf( fd, "/%d", arity(*pp++));
	    break;

	    caseop(I_NI):
		dprintf( fd, "%d ",  *pp++);
	    print_int(fd, *pp++);
	    break;

	    caseop(I_NR):
		dprintf( fd, "%d ", *pp++ );
	    *pd = *pp++;
	    *(pd+1) = *pp++;
	    dprintf( fd, "%f ", d);
	    break;

	    caseop(I_2NE):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_2NF):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    print_functor(fd, nms, *pp);
	    dprintf( fd, "/%d", arity(*pp++));
	    break;

	    caseop(I_NFE):
		dprintf( fd, "%d ",  *pp++);
	    print_functor(fd, nms, *pp);
	    dprintf( fd, "/%d ", arity(*pp++));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_2NFE):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    print_functor(fd, nms, *pp);
	    dprintf( fd, "/%d ", arity(*pp++));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_3NE):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_3NFE):
		dprintf( fd, "%d", *pp++ );
	    dprintf( fd, "%d", *pp++ );
	    dprintf( fd, "%d", *pp++ );
	    print_functor(fd, nms, *pp);
	    dprintf( fd, "/%d", arity(*pp++));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_4E):
		dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_N4E):
		dprintf( fd, "%d ",  *pp++);
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    dprintf( fd, "E%d ", ((int*)*pp++ - base));
	    break;

	    caseop(I_HTAB):		// hashtable
		op = *pp++;
	    dprintf( fd, "%d E%d \n", op, ((int*)*pp++ - base));
	    dprintf( fd, "%d pairs of table entries\n", op);

	    for (i = 0; i < op; i++){
		if (*pp == 0){
		    dprintf( fd, "VAR ");
		    pp++;
		}
		else print_constant(fd, nms,*pp++);
		dprintf( fd, " E%d ", ((int*)*pp++ - base));
		//if (i%10 == 0) dprintf( fd, "\n");
	    }
	    dprintf( fd, "%d pairs has been printed\n", i);
	    break;
	}
	dprintf( fd, "\n");
    }
}

void trace_control(ICBPtr ip, int op, int flag){
    if (ip->call_count >= ip->start_num || flag == 0){
	if (!is_messenger(ip))
	    printf("%s: %s       ", ip->name, opcode_tab[op].name);
	else
	    printf("MS(%s): %s       ", ip->name, opcode_tab[op].name);
	fflush(stdout);
    }
    if (flag) printf("\n");
}

void trace_call(ICBPtr ip, int* pp, int* st, int flag){
    char c;

    ip->call_seq++;
    if (flag == TRACE_CALL) ip->mid_call++;
    if (ip->call_count++ >= ip->start_num){
	printf("%d", ip->call_count);
	if (flag == TRACE_CALL) printf(" CALL %d: ", ip->call_seq);
	else printf(" LCALL %d: ", ip->call_seq);
	print_goal(fileno(stdout), ip->name_base, pp, st);
	c = getchar();
	fflush(stdout);
    }
}

void trace_return(ICBPtr ip, int flag){ // flag = 0/1 -> proceed/return

    if (flag == TRACE_PROCEED) ip->call_seq = ip->mid_call;

    if (ip->call_count > ip->start_num){
	if (flag == TRACE_RETURN)
	    if (!is_messenger(ip))
		printf("%s: RETURN TO %d\n",ip->name, ip->call_seq);
	    else
		printf("MS(%s): RETURN TO %d\n",ip->name, ip->call_seq);
	else 
	    if (!is_messenger(ip))
		printf("%s: PROCEED TO %d\n",ip->name, ip->call_seq);
	    else
		printf("MS(%s): PROCEED TO %d\n",ip->name, ip->call_seq);
    }
    ip->call_seq = --ip->mid_call;
}


