/** $RCSfile: parser.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: parser.h,v 1.5 2003/03/25 21:05:08 hliang Exp $
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

#define MAXPRI	1200            /* Number of operator priority levels */
#define N_OP_KIND 3
#define BINARY    0
#define POSTFIX	  1
#define PREFIX    2
#define NONE      3
#define FIRST_VAR (1)
#define SEEN_VAR (2)
#define ANONYMOUS_VAR (3)
#define MAXFUNCTORS		8000
#define HASH_TAB_NUM 		3073

struct OPERATOR {
	struct {
	int	pri,		   /* priority of operator */
	    left,		   /* priority of left operand */
	    right,		   /* priority of right operand */
		conflict;
	} ops[N_OP_KIND];
};

typedef struct imagoterm_node { /*** parse term ***/
	char ty, create_var, extra_flag;
	int argunum, val;
	char *str;
	double rval;
	struct imagoterm_node *next, *first;
} IMAGOTERM;

struct HASH_TAB_NODE {
	char ty;	/* type of the term */
	char extra;
	IMAGOTERM	*t;	/* pointer to typical term */
	int val;	/* contains either functor value or variable index */
	int argunum;	/* number of arguments in term */
	int var_index;
	char *s;	/* string of the term */
	double rval;	/* value of real number */
	struct HASH_TAB_NODE *link,*next;	/* 2D link pointers */
	struct OPERATOR *op;
	int special;
	int quick_hash;
};

typedef struct namerec {
	char arity;
	char *pname;
	int special;
} NAMEREC;

typedef struct clause {
	IMAGOTERM *t, *wt;
	struct clause *next;
	int id;
	int startcode, endcode;
} CLAUSE, *CLASUEPOINTER;

typedef struct hash_clause_node {
	struct hash_clause_node *next;
	int name;
	int n_clauses;
	CLAUSE *first_clause, *last_clause, *last_compiled_clause;
	int start_hdrcode, end_hdrcode; /*** end_hdrcode == 0 for nonheaders ***/
	SPNODE *spred;
	int flags;
	struct hash_l_node *jumimago_occ;
	double counter;
} HASH_CLAUSE_NODE, *HASH_CLAUSE_NODE_POINTER;


/*
 *	Macros for quick identification of tokens
 */
#define head_of_clause(r) (((r)->ty == FUNC && (r)->val == RULEIFVAL) ? (r)->first : (r))

/* Style checking flags */

#define H_COMPILED  (0x1)
#define H_PROTECTED (0x4)
#define H_CUT       (0x10)
#define H_ADDED     (0x20)
#define UNSER_DEF_PREDICATE 250
#define BUILTINPREDICATENUM (UNSER_DEF_PREDICATE)

#define DOTVAL	2
#define COMMAVAL	3
#define RULEIFVAL	4   /* :- */
#define NILLISTVAL 5
#define SIMECOLONVAL 6
#define DIRECTIVE 7   /* the directive :- */

#define PLUSVAL (20)
#define U_MINUSVAL (21)
#define MINUSVAL (22)
#define MULTVAL (23)
#define DIVVAL (24)
#define MODVAL (25)
#define SINVAL (26)
#define COSVAL (27)

#define ABSVAL (28)
#define MINVAL (29)
#define MAXVAL (30)
#define POWVAL (31)
#define EVALVAL (32)
#define QUOTEVAL (33)
#define ARCSINVAL (34)
#define ARCCOSVAL (35)
#define EQVAL (40)   /* =:= */
#define LTVAL (41)   /* < */
#define GTVAL (42)   /* > */
#define LEVAL (43)   /* <= */
#define GEVAL (44)   /* >= */
#define NEVAL (45)     /* =/= */
#define TNONIDENTICAL (66)    /*/==*/
#define TPRECEDE (67)         /*@< */
#define TPREIDENTICAL (68)        /*@=< */
#define TFOLLOW (69)        /* @>*/
#define TFLOIDENTICAL (70)        /*@>= */
#define TIDENTICAL (71)    /* ==*/
#define UEQVAL (72)    /* = */
#define UNEVAL (73)    /* /= */
/*#define RETRACTVAL (102)*/
#define CLAUSEVAL (103)
#define WRITEVAL (107)
#define FAILVAL (111)
#define CUTVAL  (113)
#define CALLXVAL  (114)
#define TRUEVAL  (115)

#define IMAGO_LIB_START (120)
