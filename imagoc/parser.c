/** $RCSfile: parser.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: parser.c,v 1.7 2003/03/25 21:05:08 hliang Exp $
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


#include <strings.h>
#include <ctype.h>
#include <setjmp.h>
#include "builtins.h"
#include "parser.h"
#include "lib.h"

#define NULLPT ((IMAGOTERM *) 0)
#define HASH_FUNC_NUM (2003)
#define HASH_VAR_NUM (2003)
#define INIT_HASH_SPACE	(2000)
#define CLAUSE_BLOCK (80)
#define IMAGOTERM_BLOCK	(80)
#define STR_TAB_NUM	(4000)

typedef struct imagonamenode{
	char* filename;
	struct imagonamenode *next;
	int imagotype;
} IMAGONAMENODE;

static IMAGOTERM *make_oimago_func();
static int check_clause();
static int check_pred();
static int hash_size = 0;
static CLASUEPOINTER free_clause_list = (CLASUEPOINTER) NULL;
static CLASUEPOINTER tmimago_free_clause_list = (CLASUEPOINTER) NULL;
static char *start_clause = (char *) NULL;
static int unused_clause = 0;
static IMAGOTERM *free_list = (IMAGOTERM *) NULL;
static char *start_term = (char *) NULL;
static int unused = 0;
static char malloc_space[PRE_MALLOC_MEM];
static char *cur_malloc = malloc_space;
static char *malloc_high = &malloc_space[PRE_MALLOC_MEM - 128];
static char str_table[STR_TAB_NUM];
static char *max_str_table= &str_table[STR_TAB_NUM-2];
static char *str_ptr=str_table;
int no_var_imagoterm; /* number of useful variables in imagoterm */
static int var_cnt = 0;	/* max variables in current term */
static struct HASH_TAB_NODE *used_vars[MAXVAR]; /* used hnodes in current term */
static int used_var_cnt = 0; /* unique value for variable names */
static int var_hash_id = 1;
static HASH_CLAUSE_NODE_POINTER last_imago_node = NULL;
static int last_name = -1;


extern int old_len, line_pos, lineno ;
extern char tokenchar;
extern int tokentype;
extern IMAGOTERM* tokenpointer;
extern char ch;
extern FILE *outfile;
extern int comma_flg, bar_flg;
extern char MASMFILE[];
extern FILE *errormesg;
extern IMAGONAMENODE *imagonamehead;
extern FILE *TEMP_FILE;
extern int GENFILE;

HASH_CLAUSE_NODE_POINTER hash_imago_table[HASH_TAB_NUM + 1];
NAMEREC functors[MAXFUNCTORS];
SPNODE *spred[BUILTINPREDICATENUM];
struct HASH_TAB_NODE *hnode_emptylist;
char *IMP_SUFFIX;
int real_val;
IMAGOTERM *null_list;	/* setup some predefined stuff for parser */
char *cons_str;		/* dito */
struct HASH_TAB_NODE *hash_name_tab[HASH_FUNC_NUM];
struct HASH_TAB_NODE *hash_var_tab[HASH_VAR_NUM];
struct HASH_TAB_NODE *tok_node; /* current hash node returned by lookuphashtab() */
static struct HASH_TAB_NODE hash_space[INIT_HASH_SPACE];
struct HASH_TAB_NODE *hash_base = hash_space;
int cur_func_id = UNSER_DEF_PREDICATE; /* lower bound for user-def vals */
jmp_buf err_getimagoterm;


IMAGOTERM *get_imagoterm(int);
/*CLASUEPOINTER imago_first_clause(int);*/
CLASUEPOINTER store_clause(IMAGOTERM*, IMAGOTERM*, int);
IMAGOTERM *quote_expand(IMAGOTERM* , char);

/*
 * 	add a create functor called "name" with "argunum" arguments of type "type"
 *	with value val, used to predefine functors
 *	Not allowed to redefine a name with the same arity
 */
struct {
	char *name;
	int argunum;
	int val;
} builtin_operator[] = {
	{".", 2, DOTVAL},
	{"|", 2, 4},
	{",", 2, COMMAVAL},
	{":-", 2, RULEIFVAL},
	{"[]", 0, NILLISTVAL},
	/*{"<stdin>", 0, STDINVAL},
	{"<stdout>", 0, STDOUTVAL},*/
	{":-", 1, DIRECTIVE},
	{";", 2, SIMECOLONVAL},
	{"+", 2, PLUSVAL},
	{"-", 1, U_MINUSVAL},
	{"-", 2, MINUSVAL},
	{"*", 2, MULTVAL},
	{"mod", 2, MODVAL},
	{"/", 2, DIVVAL},
	{"sin", 1, SINVAL},
	{"cos", 1, COSVAL},
	{"abs", 1, ABSVAL},
	{"min", 2, MINVAL},
	{"max", 2, MAXVAL},
	{"eval", 1, EVALVAL},
	{"quote", 1, QUOTEVAL},
	{"arcsin", 1, ARCSINVAL},
	{"arccos", 1, ARCCOSVAL},
	{"=:=", 2, EQVAL},
	{"=/=", 2, NEVAL},
	{"<", 2, LTVAL},
	{">", 2, GTVAL},
	{"<=", 2, LEVAL},
	{">=", 2, GEVAL},
	{"==", 2, TIDENTICAL},
	{"=", 2, UEQVAL},
	{"/=", 2, UNEVAL},
	{"/==", 2, TNONIDENTICAL},
	{"@<", 2, TPRECEDE},
	{"@=<", 2, TPREIDENTICAL},
	{"@>", 2, TFOLLOW},
	{"@>=", 2, TFLOIDENTICAL},
	{"clause", 2, CLAUSEVAL},
	{"fail", 0, FAILVAL},
	{"!", 0, CUTVAL},
	{"true", 0, TRUEVAL},
	{(char *) 0, 0, 0}
};

char *emalloc(n)
int n;
{
char *p;
register char *tmp;
register int j;
int pad;

	if ((int) (cur_malloc + n) < (int) malloc_high) {
		pad=((unsigned int) cur_malloc) % sizeof(double);
		if (pad) p = cur_malloc + sizeof(double) - pad;
		else p = cur_malloc;
		cur_malloc = p + n;
		return(p);
	}
	if ((p = (char *) malloc(n+8 /** TEMPORARY **/)) == NULL)
		fatalerror("Not enough memory (calloc)");
pad=((unsigned int) p) % sizeof(double);
if (pad) {
p += sizeof(double) - pad;
}
	return(p);
}
/*
 *	Puts s into the string table and returns the string table address
 */
char *addstrtable(s)
char *s;
{
	char *start;
	register char *s1;

PUT_STRING:
	s1 = s;
	start = str_ptr;
	while (*s1 && str_ptr < max_str_table)
		*str_ptr++ = *s1++;
	if (*s1 && str_ptr >= max_str_table) {
		str_ptr = emalloc(STR_TAB_NUM);
		max_str_table = str_ptr + STR_TAB_NUM - 2;
		goto PUT_STRING;
	}
	*str_ptr++ ='\0';
	return(start);
}


struct HASH_TAB_NODE *create_hashtabnode()
{
struct HASH_TAB_NODE *p;
HASH_AGAIN:
	if (hash_size < INIT_HASH_SPACE) p = &hash_base[hash_size++];
	else {
		fatalerror("no more HASH_TAB_NODE space");
	}
	p->next = p->link = NULL;
	p->quick_hash = 0;
	p->t = NULL;
	p->op = NULL;
	p->extra = (char) FALSE;
	return(p);
}

IMAGOTERM *create_imagoterm()
{
	IMAGOTERM *t;
 	if (free_list) {
 		t = free_list;
 		free_list = t->next;
 	} else if (unused > 0) {
 		unused--;
 		t = (IMAGOTERM *) start_term;
 		start_term += sizeof(IMAGOTERM);
 	} else {
 		unused = IMAGOTERM_BLOCK;
 		start_term = emalloc(IMAGOTERM_BLOCK * sizeof(IMAGOTERM));
 		unused--;
 		t = (IMAGOTERM *) start_term;
 		start_term += sizeof(IMAGOTERM);
 	}
 	t->next = t->first = NULL;
 	return(t);
 }

numfunc(val, arity, s, special)
int val;
int arity;
char *s;
int special;
{
 	functors[val].arity = arity;
 	functors[val].pname = s;
 	functors[val].special = special;
}

IMAGOTERM *create_pt_emptylist()
{
	IMAGOTERM *t;
  	t = create_imagoterm();
 	t->str = null_list->str;
  	t->val = null_list->val;
  	t->argunum = 0;
  	t->ty = FUNC;
  	return t;
}


struct HASH_TAB_NODE *addhashnametab(name, argunum, val)
char *name;
int argunum, val;
{
int h, size, quick_cmp, found=FALSE;
struct HASH_TAB_NODE *cur;
IMAGOTERM *p, *t;
struct OPERATOR *op;
	quick_cmp = stringsum(name);
	if ((cur = hash_name_tab[h=hash(name, HASH_FUNC_NUM)]) != NULL)
		for (; cur != NULL; cur = cur->next)
			if (cur->quick_hash == quick_cmp && !strcmp(cur->s, name)) {
				found = TRUE;
				break;
			}

	if (!found) {
		cur = create_hashtabnode();
		cur->quick_hash = quick_cmp;
		cur->s = addstrtable(name);
		cur->val = val;
		if (hash_name_tab[h] == NULL)
			hash_name_tab[h] = cur;
		else {
			cur->next = hash_name_tab[h];
			hash_name_tab[h] = cur;
		}
	}

	for (p = cur->t; p; p = p->next)
		if (p->argunum == argunum)
			if (p->val == val)
				fatalerror("redefining initialised functor as the same");
			else {
				printf("name %s argunum %d val %d ty %d\n",
					name, p->argunum, p->val, p->ty);
				fatalerror("redefining initialised functor differently");
			}
	t = create_imagoterm();
	t->argunum = argunum;
	t->val = val;
	t->next = cur->t;
	cur->t = t;

	if (!(op = cur->op)) numfunc(val, argunum, name, NONE);
	else if (argunum == 2) {
		if (op->ops[BINARY].pri >= 0) numfunc(val, argunum, name, BINARY);
		else fatalerror("addhashnametab special wrong");
	} else if (argunum == 1) {
		if (op->ops[PREFIX].pri >= 0) numfunc(val, argunum, name, PREFIX);
		else if (op->ops[POSTFIX].pri >= 0) numfunc(val, argunum, name, POSTFIX);
		else fatalerror("addhashnametab special wrong");
	}
}



void addbuiltin(char *s, int n, void (*f)(), SPNODE *pp, int val) {
	int i;

	addhashnametab(s, n, val);
	i = hashbuiltina(s,n);
	if (i != val) fatalerror("92018");
	assign_imago_protected(i, TRUE);
	spred[i] = pp;
}

void init_all() {
	init_malloc();
	init_name();
	init_hash_imago_table();
	real_val = hashbuiltina("real", 1);
	IMP_SUFFIX = DEF_IMP_SUFFIX;
}

struct HASH_TAB_NODE *insert_name(hash_tab, sym, quick_cmp, size)
struct HASH_TAB_NODE *hash_tab[];
char *sym;
int quick_cmp, size;
{
int h;
struct HASH_TAB_NODE *cur, *p1;
	if ((cur = hash_tab[h=hash(sym, size)]) != NULL)
		for (; cur != NULL; cur = cur->next) {
			if (cur->quick_hash == quick_cmp && !strcmp(cur->s, sym)) {
				return cur;
			}
		}
	cur = create_hashtabnode();
	cur->quick_hash = quick_cmp;
	cur->s = addstrtable(sym);
	cur->val = cur_func_id++;
	if (hash_tab[h] == NULL) hash_tab[h] = cur;
	else {
	  	cur->next = hash_tab[h];
		hash_tab[h] = cur;
	}
	return cur;
}

IMAGOTERM *lookuphashtab(sym, quick_cmp)
char *sym;
int quick_cmp;
{
struct HASH_TAB_NODE *hnode;
IMAGOTERM *t;
	hnode = (struct HASH_TAB_NODE *)insert_name(hash_name_tab, sym, quick_cmp, HASH_FUNC_NUM);
	t = create_imagoterm();
	t->str = hnode->s;
	t->val = hnode->val;
	t->ty = FUNC;
	t->argunum = -1;
	tok_node = hnode;
	return t;
}


IMAGOTERM *argunum_imagoterm(hnode, t, argunum) /* correct val and argunum? */
struct HASH_TAB_NODE *hnode;
IMAGOTERM *t;
int argunum;
{
IMAGOTERM *p, *p1;
int val, found = FALSE;
struct OPERATOR *op;
	if (t->ty != FUNC) fatalerror("57233");
	t->argunum = argunum;
	for (p1 = hnode->t; p1; p1 = p1->next)
		if (p1->argunum == argunum) {
			found = TRUE;
			break;
		}
	if (found) t->val = p1->val;
	else {
		p = create_imagoterm();
		p->argunum = argunum;
		p->next = hnode->t;
		hnode->t = p;
		if (hnode->t) t->val = p->val = cur_func_id++;
		else t->val = p->val = hnode->val;
	}
	if (!(op=hnode->op)) numfunc(t->val, argunum, t->str, NONE);
	else if (argunum == 2) {
		if (op->ops[BINARY].pri >= 0) numfunc(t->val, argunum, t->str, BINARY);
		else numfunc(t->val, argunum, t->str, NONE);
	} else if (argunum == 1) {
		if (op->ops[PREFIX].pri >= 0) numfunc(t->val, argunum, t->str, PREFIX);
		else if (op->ops[POSTFIX].pri >= 0)
			numfunc(t->val, argunum, t->str, POSTFIX);
		else numfunc(t->val, argunum, t->str, NONE);
	} else numfunc(t->val, argunum, t->str, NONE);

	return t;
}

IMAGOTERM *func_argunum_imagoterm(t, argunum)
IMAGOTERM *t;
int argunum;
{
struct HASH_TAB_NODE *hnode;
	hnode = (struct HASH_TAB_NODE *)insert_name(hash_name_tab, t->str, stringsum(t->str), HASH_FUNC_NUM);
	return((IMAGOTERM *)argunum_imagoterm(hnode, t, argunum));
}


init_name() /*** predefined names for parser ***/
{
IMAGOTERM *t;
int i, lib_val;

	for (i = 0; builtin_operator[i].name; i++) {
		addhashnametab(builtin_operator[i].name,
		         builtin_operator[i].argunum,
                 builtin_operator[i].val);
	}
	null_list = (IMAGOTERM *) lookuphashtab("[]", stringsum("[]"));
	/*lookuphashtab("[]", stringsum("[]"));*/
	hnode_emptylist = tok_node;
	func_argunum_imagoterm(null_list, 0);
	t = (IMAGOTERM *)lookuphashtab(".", stringsum("."));
	cons_str = t->str;

	operator_add("is",   "xfx", 40);
	operator_add(".",   "xfy", 51);
	operator_add(",",  "xfy", 252);
	operator_add(":-",  "xfx", 255);
	operator_add("-",   "fy",  21);
	operator_add(":-",  "fx",  255);
	operator_add("-",   "yfx", 31);
	operator_add("=",   "xfx", 40);
	operator_add("->",   "xfy", 254);
	operator_add(";",   "xfy", 253);
	operator_add("'",   "fx", 50);
	operator_add("+",   "yfx", 31);
	operator_add("/",   "yfx", 21);
	operator_add("*",   "yfx", 21);
	operator_add(">",   "xfx", 37);
	operator_add(">=",   "xfx", 37);
	operator_add("<",   "xfx", 37);
	operator_add("<=",   "xfx", 37);
	operator_add("==",   "xfx", 37);
	operator_add("mod",   "yfx", 21);
	operator_add("=/=",   "yfx", 37);
	operator_add("=:=",   "xfx", 37);
	operator_add("/=",   "xfx", 40);
	operator_add("/==.",   "xfx", 40);
	operator_add("@<",   "xfx", 40);
	operator_add("@=<",   "xfx", 40);
	operator_add("@>",   "xfx", 40);
	operator_add("@>=",   "xfx", 40);

	for (i = 0, lib_val = IMAGO_LIB_START; btin_tab[i].str; i++, lib_val++) {
		addbuiltin(btin_tab[i].str, btin_tab[i].arity,
		         btin_tab[i].bf, &btin_tab[i], lib_val);
	}
	if (lib_val >= UNSER_DEF_PREDICATE) fatalerror("64209");


}

init_malloc()
{
unsigned int pad;
	pad=((unsigned int) cur_malloc) % sizeof(double);
	if (pad > 16)
		warning("Strange alignment found %d (*** May be a bug ***)\n",pad);
	if (pad) cur_malloc += sizeof(double) - pad;
}

/*
 *	top level term reader
 *		returns NULLPT for syntax errors or EOF
 *	terms must always end with a '.'
 */

IMAGOTERM *getimagoterm()
{
	IMAGOTERM *t;
	comma_flg = bar_flg = FALSE;
	if (setjmp(err_getimagoterm))
		return(NULLPT);
	zero_vars();
	imagoscan();
	if (tokentype == ENDFILE) return(NULLPT);
	t=get_imagoterm(MAXPRI);
	if (is_punc('.'))
		if (t == NULLPT)
			error_syntax("expected a term");
		else	return(t);
	else if (tokentype == ENDFILE) {
		error_syntax("unexpected end of file in term");
		return(NULLPT);
	} else {
		error_syntax("unexpected symbol (expected '.')");
		return(NULLPT); /* quiet gcc */
	}
}

HASH_CLAUSE_NODE_POINTER find_hash_clause_node(name)
int name;
{
register HASH_CLAUSE_NODE_POINTER hp;
int i;
	if (name == last_name) return last_imago_node;
	i = hash_p(name);
	hp = hash_imago_table[i];
	while (hp) {
		if (hp->name == name) {
			last_name = name;
			last_imago_node = hp;


			return hp;
		}
		hp = hp->next;
	}
	return hp;
}

CLASUEPOINTER imago_first_clause(name)
int name;
{
HASH_CLAUSE_NODE_POINTER hp;
int i;
	hp = find_hash_clause_node(name);
	if (hp) return hp->first_clause;
	else return (CLASUEPOINTER) NULL;
}



CLASUEPOINTER create_clause()
{
CLASUEPOINTER r;
	if (free_clause_list) {
		r = free_clause_list;
		free_clause_list = r->next;
	} else if (unused_clause > 0) {
		unused_clause--;
		r = (CLASUEPOINTER) start_clause;
		start_clause += sizeof(CLAUSE);
	} else {
		unused_clause = CLAUSE_BLOCK;
		start_clause = (char *) emalloc(CLAUSE_BLOCK * sizeof(CLAUSE));
		unused_clause--;
		r = (CLASUEPOINTER) start_clause;
		start_clause += sizeof(CLAUSE);
	}
	r->next = NULL;
	r->endcode = -1; /* uncompiled state */
	return r;
}

CLASUEPOINTER imago_last_clause(name)
int name;
{
HASH_CLAUSE_NODE_POINTER hp;
int i;
	hp = find_hash_clause_node(name);
	if (hp) return hp->last_clause;
	else return (CLASUEPOINTER) NULL;
}

CLASUEPOINTER store_clause(to, t, clobber) /*** free to eventually ***/
IMAGOTERM *to, *t;
int clobber;
{
int val;
CLAUSE *r, *tr;
	val = (((t)->ty == FUNC && (t)->val == RULEIFVAL) ? t->first->val : t->val);
	r = create_clause();
	r->next = NULL;
	r->t = to;
	r->wt = t;
	if (!imago_first_clause(val)) {	/* n_clauses not used for dynamic */
		assign_imago_first_clause(val, r);
		assign_imago_last_clause(val, r);
		assign_imago_n_clauses(val, 1);
		r->id = 1;
	} else {
		tr = imago_last_clause(val);
		tr->next = r;
		if (tr->id == -1) r->id = 1;
		else r->id = tr->id + 1;
		assign_imago_n_clauses(val, imago_n_clauses(val) + 1);
		assign_imago_last_clause(val, r);
	}
	return r;
}
IMAGOTERM *duplicate_imagoterm(t)
IMAGOTERM *t;
{
IMAGOTERM *nt;
int i;
char *c1, *c2;
	nt = (IMAGOTERM *) create_imagoterm();
	memcpy(nt, t, sizeof(IMAGOTERM));
	return nt;
}

IMAGOTERM *quote_expand(t, quoted)
IMAGOTERM *t;
char quoted;
{
int v, n, *start, *s;
IMAGOTERM *arg, *createt, *createarg, *prev;
	switch (t->ty) {
	case REAL:
	case STR:
	case VAR:
		return duplicate_imagoterm(t);
	case FUNC:
		if (((t)->ty == FUNC && (t)->val == QUOTEVAL) && !quoted) return quote_expand(t->first, TRUE);
		if (t->argunum == 0) return duplicate_imagoterm(t);
		createt = duplicate_imagoterm(t);
		createt->first = quote_expand(t->first, quoted);
		arg = t->first;
		createarg = createt->first;
		for (n = 2;	n <= t->argunum; n++) {
			createarg->next = quote_expand(arg->next, quoted);
			arg = arg->next;
			createarg = createarg->next;
		}
		return createt;
	default:
		fatalerror("47812");
		return (IMAGOTERM *) NULL; /* quiet gcc */
	}
}

readsourcefile(clobber) /*** low-level read ***/
int clobber;
{
IMAGOTERM *tt,*ttex;
HASH_CLAUSE_NODE_POINTER hhpp;
int i, *tmp1;
int found_a_cut, found_a_quote;
int prev_clause_val = 0, cur_clause_val;

	while (tt = (IMAGOTERM *) getimagoterm()) {
		found_a_cut = FALSE;
		if (!check_clause(tt, &found_a_cut, &found_a_quote))
			break;
		ttex = (found_a_quote ? quote_expand(tt, FALSE) : tt);
		if (found_a_cut)
			assign_imago_cut((((tt)->ty == FUNC && (tt)->val == RULEIFVAL) ? tt->first->val : tt->val), TRUE);
			cur_clause_val = head_of_clause(ttex)->val;
			if (prev_clause_val != cur_clause_val) {
				if (store_clause(tt, ttex, clobber)) prev_clause_val = cur_clause_val;
			} else store_clause(tt, ttex, clobber);
			assign_imago_added(tt->val, TRUE);
	}
}

imago_n_clauses(name)
int name;
{
HASH_CLAUSE_NODE_POINTER hp;
int i;
	hp = find_hash_clause_node(name);
	if (hp) return hp->n_clauses;
	else return 0;
}

really_free_clauses()
{
	CLASUEPOINTER prev = (CLASUEPOINTER) NULL, r;

	for (r = tmimago_free_clause_list; r; prev = r,r = r->next);
	if (prev) {
		prev->next = free_clause_list;
		free_clause_list = tmimago_free_clause_list;
		tmimago_free_clause_list = (CLASUEPOINTER) NULL;
	}
}

static int check_clause(pt, found_cut, found_quote)
IMAGOTERM *pt;
int *found_cut;
int *found_quote;
{
IMAGOTERM *body;
int dummy;
	*found_quote = FALSE;
	if (pt->ty==FUNC && pt->val==RULEIFVAL) {
		if (!check_pred(pt->first, &dummy, found_quote))
			return FALSE;
		for (body = pt->first->next; (body->ty==FUNC && body->val==COMMAVAL);
				body = body->first->next)
			if (!check_pred(body->first, found_cut, found_quote))
				return FALSE;
		if (!check_pred(body, found_cut, found_quote)) return FALSE;
	} else if (!check_pred(pt, found_cut, found_quote)) return FALSE;
	return TRUE;
}

static int check_quote(pt)
IMAGOTERM *pt;
{
int n;
IMAGOTERM *arg;
	if ((((pt)->ty == FUNC) && ((pt)->argunum))) {
		if (((pt)->ty == FUNC && (pt)->val == QUOTEVAL)) {
			return TRUE;
		}
		n = pt->argunum;
		for (arg = pt->first; n > 0; n--, arg = arg->next)
			if (check_quote(arg)) return TRUE;
	}
	return FALSE;
}

static int check_pred(pt, found_cut, found_quote)
IMAGOTERM *pt;
int *found_cut;
int *found_quote;
{
	if (((pt)->ty == REAL) || ((pt)->ty == VAR) || ((pt)->ty == STR)) {
		fprintf(outfile, "Syntax error in ");
		fprintf(outfile, ".\n");
		return FALSE;
	} else if (((pt)->ty == FUNC && (pt)->val == CUTVAL) && !(*found_cut)) *found_cut = TRUE;
	else if (!*found_quote) *found_quote = check_quote(pt);
	return TRUE;
}

IMAGOTERM *get_args(hnode, func)
struct HASH_TAB_NODE *hnode;
IMAGOTERM *func;
{
	int i=0,n=0;
	IMAGOTERM *t,*prev;
	int oldcomma;

	oldcomma=comma_flg;
	comma_flg=TRUE;
	imagoscan();
	if (!(t=get_imagoterm(MAXPRI)))
		error_syntax("expected first argument");
	func->first=t;
	for (prev=t,n=1; is_punc(','); prev->next=t,prev=t,n++) {
		imagoscan();
		if (!(t=get_imagoterm(MAXPRI)))
			error_syntax("expected argument");
	}
	if (tokentype != PAREN || tokenchar != ')')
		error_syntax("expected ')' to close functor");
	comma_flg=oldcomma;
	imagoscan();
	return((IMAGOTERM *)argunum_imagoterm(hnode,func,n));
}

IMAGOTERM *get_nfix()
{
	fatalerror("botch get_nfix");
}

IMAGOTERM *get_list()
{
	IMAGOTERM *t,*t1,*t2=NULLPT;

	imagoscan();
	if ((t1=get_imagoterm(MAXPRI)) == NULL)
		error_syntax("malformed list head");
	if (is_punc('|')) {
		imagoscan();
		if ((t2=get_imagoterm(MAXPRI)) == NULL)
			error_syntax("malformed list tail");
	} else if (is_punc(',')) t2=get_list();
	else if (tokentype != PAREN || tokenchar != ']')
		error_syntax("expected one of {'|',',',']'} in list");
	t = create_imagoterm();
	t->str = cons_str;
	t->argunum = 2;
	t->val = DOTVAL;
	t->ty = FUNC;
	t->first = t1;
	if (t2) t1->next = t2;
	else t1->next = (IMAGOTERM *)create_pt_emptylist();
	return(t);
}


HASH_CLAUSE_NODE_POINTER malloc_null_HASH_CLAUSE_NODE()
{
HASH_CLAUSE_NODE_POINTER p;
	p = (HASH_CLAUSE_NODE_POINTER) calloc(1, sizeof(HASH_CLAUSE_NODE));
	p->counter = 0.0;
	return p;
}

init_hash_imago_table()
{
int i;
	for (i = 0; i <= HASH_TAB_NUM; i++) hash_imago_table[i] = NULL;
}

hash_p(name)
int name;
{
	return ((name % HASH_TAB_NUM) + 1); /*** 1 <= hash_value <= HASH_TAB_NUM ***/
}



HASH_CLAUSE_NODE_POINTER create_hash_clause_node(name) /*** may or may not malloc ***/
int name;
{
HASH_CLAUSE_NODE_POINTER hp0, hp;
int i;
	if (name == last_name) return last_imago_node;
	i = hash_p(name);
	hp = hp0 = hash_imago_table[i];
	while (hp) {
		if (hp->name == name) {
			last_name = name;
			last_imago_node = hp;
			return hp;
		}
		hp = hp->next;
	}
	hp = (HASH_CLAUSE_NODE_POINTER) malloc_null_HASH_CLAUSE_NODE();
	hp->next = hp0;
	hp->name = name;
	hash_imago_table[i] = hp;
	return hp;
}


imago_compiled(name)
int name;
{
HASH_CLAUSE_NODE_POINTER hp;
int i;
	hp = find_hash_clause_node(name);
	if (hp) return hp->flags & H_COMPILED;
	else return FALSE;
}


assign_imago_first_clause(name, clause)
int name;
CLASUEPOINTER clause;
{
HASH_CLAUSE_NODE_POINTER hp;
	hp = create_hash_clause_node(name);
	hp->first_clause = clause;
}

assign_imago_last_clause(name, clause)
int name;
CLASUEPOINTER clause;
{
HASH_CLAUSE_NODE_POINTER hp;
	hp = create_hash_clause_node(name);
	hp->last_clause = clause;
}


assign_imago_n_clauses(name, i)
int name, i;
{
HASH_CLAUSE_NODE_POINTER hp;
	hp = create_hash_clause_node(name);
	hp->n_clauses = i;
}


assign_imago_protected(name, flag)
int name, flag;
{
HASH_CLAUSE_NODE_POINTER hp;
	hp = create_hash_clause_node(name);
	if (flag) hp->flags |= H_PROTECTED;
	else hp->flags &= ~H_PROTECTED;
}


assign_imago_cut(name, flag)
int name, flag;
{
HASH_CLAUSE_NODE_POINTER hp;
	hp = create_hash_clause_node(name);
	if (flag) hp->flags |= H_CUT;
	else hp->flags &= ~H_CUT;
}

assign_imago_added(name, flag)
int name, flag;
{
HASH_CLAUSE_NODE_POINTER hp;
	hp = create_hash_clause_node(name);
	if (flag) hp->flags |= H_ADDED;
	else hp->flags &= ~H_ADDED;
}


IMAGOTERM *get_imagoterm(n)
int n;
{
IMAGOTERM *t, *exp[2];
int oldpri, pri, left, right;
int binflg;
int old_comma, old_bar;
struct HASH_TAB_NODE *hnode, *hnode1;

	switch (tokentype) {
	case NAME:
		exp[0] = tokenpointer;
		hnode = tok_node;
		imagoscan();
		if (is_operator_node(hnode, PREFIX, &pri, &left, &right)) {
			if (tokentype == REAL && exp[0]->val == U_MINUSVAL) {
				exp[0] = tokenpointer;
				tokenpointer->rval = -tokenpointer->rval;
				imagoscan();
				break;
			}
			if (n >= pri) {
				t = exp[0];
				if (!(exp[0] = get_imagoterm(right)))
					return((IMAGOTERM *)argunum_imagoterm(hnode, t, 0));
				else {
					exp[0] = make_oimago_func(hnode, t, 1, exp);
					break;
				}
			} else return((IMAGOTERM *)argunum_imagoterm(hnode,exp[0],0));
		}
		if (tokentype == PAREN && tokenchar == '(')
			exp[0] = get_args(hnode, exp[0]);
		else if (is_nfix_node(hnode))
			exp[0] = get_nfix(exp[0]);
		else argunum_imagoterm(hnode,exp[0],0);
		break;
	case STR:
	case VAR:
	case REAL:
		exp[0] = tokenpointer;
		imagoscan();
		break;
	case PAREN:
		if (tokenchar == '(') {
			old_comma = comma_flg;
			comma_flg = FALSE;
			imagoscan();
			exp[0] = get_imagoterm(MAXPRI);
			comma_flg = old_comma;
			if (tokentype != PAREN || tokenchar != ')')
				error_syntax("expected closing ')'");
			else imagoscan();
		} else if (tokenchar == '[') {
			old_bar=bar_flg;
			old_comma=comma_flg;
			comma_flg=TRUE;
			bar_flg=TRUE;
			exp[0]=get_list();
			bar_flg=old_bar;
			comma_flg=old_comma;
			if (tokentype != PAREN || tokenchar != ']')
				error_syntax("expected closing ]");
			imagoscan();
		} else return(NULLPT);
		break;
	default:
		return(NULLPT);
	}

	oldpri = 0;
	hnode = tok_node;
	while (tokentype == NAME &&
		((binflg = is_operator_node(hnode,BINARY,&pri,&left,&right))||
		is_operator_node(hnode, POSTFIX, &pri, &left, &right))) {
		if (n < pri) return(exp[0]);
		if (left >= oldpri) {
			t = tokenpointer;
			imagoscan();
			if (binflg) {
				if (!(exp[1]=get_imagoterm(right)))
					error_syntax("expected right operand");
				exp[0]=make_oimago_func(hnode,t,2,exp);
			} else exp[0]=make_oimago_func(hnode,t,1,exp);
			oldpri = pri;
		} else break;
		hnode = tok_node;
	}
	return(exp[0]);
}

static IMAGOTERM *make_oimago_func(hnode, t, argunum, exp)
struct HASH_TAB_NODE *hnode;
IMAGOTERM *t,*exp[];
int argunum;
{
int i;

	if (t->ty != FUNC || argunum > 2) fatalerror("87540");
	t->first = exp[0];
	if (argunum == 2) exp[0]->next = exp[1];
	return (IMAGOTERM *)argunum_imagoterm(hnode, t, argunum);
}

int *findbind(x)
int *x;
{
	deref(x);
	return x;
}


int bimago_null()
{
	return 1;
}

FILE *my_open(name, mode)
char *name, *mode;
{
char *suf,*start,fname[MAX_NAME_LEN+1];
FILE *fil;
int l;

	if ((fil = fopen(name, mode)) != (FILE *) NULL) return fil;
	if (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL)
		return (FILE *) NULL;
	suf=IMP_SUFFIX;
	while (*suf) {
		strcpy(fname, name);
		if (*suf == ':') suf++;
		for (l=0,start=suf; *suf != '\0' && *suf != ':'; suf++,l++);
		strncat(fname, start, l);
		if ((fil = fopen(fname, mode)) != (FILE *) NULL) return fil;
	}
	return (FILE *) NULL;
}

mlvminternal_consult(IMAGONAMENODE *inimagonamenode, char *infilename)
{
int *tmp, *nt;
if (!(cur_stream = my_open(infilename, "r"))){
		errorabort("Could not read file %s\n", inimagonamenode);
		return FALSE;
	}
	old_len = 0;
	line_pos = 0;
	lineno = 1;
	unreadchar(' ');
	readchar();
	readsourcefile(FALSE);
	compile_mlvm(FALSE, inimagonamenode, NULL);
	rewind(stdin);
	unreadchar(' ');
	readchar();
	return TRUE;
}

internal_consult(s)
char *s;
{
	printf(" I do not accept pl file right now!!!!");
	exit(0);
}

hash(s, siz)
char *s;
int siz;
{
register unsigned int h;
	for (h = 0; *s; s++) h = ((h * h) << 2) + *s;
	if ((int) h >= 0) return (int) (h % siz);
	else return (-(int) h) % siz;
}



zero_vars() /* start create terms with no variables seen yet	*/
{
int i;
	no_var_imagoterm = var_cnt = 0;
	for (i = 0; i < used_var_cnt; i++) used_vars[i]->special = -1;
	used_var_cnt = 0;
}

static struct HASH_TAB_NODE *insert_var(hash_tab, sym, quick_cmp, size)
struct HASH_TAB_NODE *hash_tab[];
char *sym;
int quick_cmp, size;
{
int h;
struct HASH_TAB_NODE *cur;
	if ((cur = hash_tab[h=hash(sym, size)]) != NULL)
		for (; cur != NULL; cur = cur->next) {
			if (cur->quick_hash == quick_cmp && !strcmp(cur->s, sym)) {
				return cur;
			}
		}
	cur = create_hashtabnode();
	cur->quick_hash = quick_cmp;
	cur->s = addstrtable(sym);
	cur->val = var_hash_id++;
	cur->special = -1;
	if (hash_tab[h] == NULL) hash_tab[h] = cur;
	else {
		cur->next = hash_tab[h];
		hash_tab[h] = cur;
	}
	return cur;
}

/*
 *	Lookup the variable var and return a term to it
 *	variables which have been seen before get the same value
 */
IMAGOTERM *lookuimago_var(var, quick_cmp)
char *var;
int quick_cmp;
{
int i;
IMAGOTERM *p, *t;
struct HASH_TAB_NODE *hnode;
int hval, var_index;
	hnode = insert_var(hash_var_tab, var, quick_cmp, HASH_VAR_NUM);
	t = create_imagoterm();
	t->ty = VAR;
	t->str = hnode->s;
	if (*var != '_' || var[1])    /* fast strcmp with "_" */
		if (hnode->special < 0) { /* first instance of var */
			no_var_imagoterm++;
			hnode->var_index = t->val = var_cnt++;
			hnode->special = 1;
			used_vars[used_var_cnt++] = hnode;
			t->create_var = FIRST_VAR;
		} else {
			t->val = hnode->var_index;
			t->create_var = SEEN_VAR;
		}
	else {
		t->val = var_cnt++;
		t->create_var = ANONYMOUS_VAR;
	}
	return t;
}


IMAGOTERM *create_real(val)
double val;
{
IMAGOTERM *t;
	t = create_imagoterm();
	t->ty = REAL;
	t->rval = val;
	t->str = "$real";
	return(t);
}

IMAGOTERM *create_str(s)
char *s;
{
IMAGOTERM *t;
	t = create_imagoterm();
	t->ty = STR;
	t->str = addstrtable(s);
	return t;
}

/*
 *	Add a create operator "sym" of type "ty" and priority "pri"
 *	left operands have priority <= left
 *	similarly for right operands
 */
addop(sym, ty, pri, left, right, readonly)
char *sym;
int ty, pri, left, right, readonly;
{
struct OPERATOR *op, *opnode;
struct HASH_TAB_NODE *hnode;
IMAGOTERM *t;
int i, n = 0;
	if (ty == BINARY) n = 2;
	else if (ty == POSTFIX || ty == PREFIX) n = 1;
	else fatalerror("21333");
	if (pri < 0) {
		warning("priority of operator %s is negative", sym);
		return;
	}
	hnode = (struct HASH_TAB_NODE *)insert_name(hash_name_tab, sym, stringsum(sym), HASH_FUNC_NUM);
	if (!(op = hnode->op)) {
		opnode = (struct OPERATOR *) emalloc(sizeof(struct OPERATOR));
		for (i=0; i < N_OP_KIND; i++)
			opnode->ops[i].pri =
				opnode->ops[i].left =
					opnode->ops[PREFIX].right = -1;
		hnode->op = opnode;
		op = opnode;
	} else if (readonly && op->ops[ty].pri >= 0) {
		warning("attempt to redefine %s (ignored)", sym);
		return;
	}
	i = hashbuiltina(sym, n);
	if (pri == 0) {
		pri = left = right = -1;
		functors[i].special = NONE;
	} else numfunc(i, n, sym, ty);
	op->ops[ty].pri = pri;
	op->ops[ty].left = left;
	op->ops[ty].right = right;
}

operator_add(sym, style, pri)
char *sym, *style;
int pri;
{
	if (strcmp(style, "xfx") == 0)
		addop(sym, BINARY, pri, pri - 1, pri - 1, FALSE);
	else if (strcmp(style, "xfy") == 0)
		addop(sym, BINARY, pri, pri - 1, pri, FALSE);
	else if (strcmp(style, "yfx") == 0)
		addop(sym, BINARY, pri, pri, pri - 1, FALSE);
	else if (strcmp(style, "yfy") == 0)
		addop(sym, BINARY, pri, pri, pri, FALSE);
	else if (strcmp(style, "fx") == 0)
		addop(sym, PREFIX, pri, 0, pri - 1, FALSE);
	else if (strcmp(style, "fy") == 0)
		addop(sym, PREFIX, pri, 0, pri, FALSE);
	else if (strcmp(style, "xf") == 0)
        addop(sym, POSTFIX, pri, pri - 1, 0, FALSE);
	else if (strcmp(style, "yf") == 0)
		addop(sym, POSTFIX, pri, pri, 0, FALSE);
	else return FALSE;
	return TRUE;
}

/*
 *	get information about operator "sym" of type "ty"
 *	returns TRUE if operator exists and sets pri, left, right
 */
is_operator_node(hnode, ty, pri, left, right)
struct HASH_TAB_NODE *hnode;
int ty, *pri, *left, *right;
{
struct OPERATOR *op;
	if (!(op = hnode->op)) return FALSE;
	if (op->ops[ty].pri >= 0 ) {
		*pri = op->ops[ty].pri;
		*left = op->ops[ty].left;
		*right = op->ops[ty].right;
		return(TRUE);
	}
	return(FALSE);
}

is_nfix_node(hnode)
struct HASH_TAB_NODE *hnode;
{
  return FALSE;
}

int hashbuiltina(s, n)
char *s;
int n;
{
IMAGOTERM *t;
struct HASH_TAB_NODE *hnode;
	hnode = (struct HASH_TAB_NODE *)insert_name(hash_name_tab, s, stringsum(s), HASH_FUNC_NUM);
	t = create_imagoterm();
	t->ty = FUNC;
	t->str = s;
	argunum_imagoterm(hnode, t, n);
	return t->val;
}

getmaxfunc()
{
	return (cur_func_id + 1);
}

