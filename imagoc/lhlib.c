 /** $RCSfile: lhlib.c,v $
  * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
  *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
  *                     H. Liang <hliang@draco.cis.uoguelph.ca>
  * $Id: lhlib.c,v 1.22 2003/03/25 21:05:08 hliang Exp $
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

#include <sys/resource.h>
#include <strings.h>

#include <ctype.h>
#include "builtins.h"
#include "parser.h"
#include "lib.h"

extern FILE *infile, *outfile, *errormesg;
extern struct HASH_TAB_NODE *hash_name_tab[];
extern struct HASH_TAB_NODE *hash_var_tab[];
extern HASH_CLAUSE_NODE_POINTER hash_imago_table[];
extern FILE *outlsfile;
extern int GENFILE;
extern int OPTIMVAL;
extern int PRINTTAB;
extern int *chtype;

typedef struct imagonamenode{
	char* filename;
	struct imagonamenode *next;
	int imagotype;
} IMAGONAMENODE;

typedef struct{
char* name;
int first;
} MLVMNODE;

typedef struct mlvmterm {
int type; /* (REF, CON/INT, FLP, LIS, STR, LVS) */
int level; /* nesting level */
int index; /* source term index */
int annot; /* annotated term index */
int argunum;  /* I add it, record the number of arguments if it is STR */
double rval; /* if type==REAL , rval will record the value */
MLVMNODE* node;
struct mlvmterm* kids;
struct mlvmterm* next;
} MLVMTERM;


typedef struct mlvmclauselink{
MLVMTERM *mlvmtermhead;
struct mlvmclauselink* next;

} MLVMCLAUSELINK;

typedef struct mathstackterm{
int type;
int intval;
double floatval;
int address;
} MLVMMATHSTACKTERM;



#define ISVAL (251)                   /* is operator */
#define IFTHENELSEVAL (253)           /* -> operator */
#define MLVMSTACKTREESIZE 1000
#define MLVMMAXVARNUM 400            /* maximum variable number in a single clause */
#define MLVMMATHARGUSIZE  50        /* arguments size for math calculation */
#define MLVMFACT 1
#define MLVMCHAIN 2
#define MLVMSPENDCLAUSE 3
#define MLVMUSERENDCLAUSE 4
#define MLVMQUERY 5
#define ACTIVATIONFRAME  3          /* additional cell for activation frame */
#define MAX_HASHENTRY_NUM (1000)  /* the hashtable entry number */

/* I do not know why I need to define these again, for they have been defined in imagoscan.c */
#define class(bits, no) ((bits << 4)| no)
#define CLASS_NO_MASK   (0xF)

#define CASE 0
#if CASE
#define mask_class_no(x)		((x) & CLASS_NO_MASK)
#define mask_class_bits(x)		((x) & ~CLASS_NO_MASK)
#else
#define mask_class_no(x)		(x)
#define mask_class_bits(x)		((x) & ~CLASS_NO_MASK)
#endif

#define UC   class(0x1,0)	  /* Upper case alphabetic */
#define UL   class(0x2,1)	  /* Underline */
#define LC   class(0x4,2)	  /* Lower case alphabetic */
#define N    class(0x8,3)	  /* Digit */
#define QT   class(0x10,4)	  /* Single quote */
#define DC   class(0x20,5)    /* Double quote */
#define SY   class(0x40,6)	  /* Symbol character */
#define SL   class(0x80,7)	  /* Solo character */
#define BK   class(0x100,8)	  /* Brackets & friends */
#define BS   class(0x200,9)	  /* Blank space */
#define ENDF class(0x400,10)  /* EOF */

#define is_math_operator(t) { (t->ty==FUNC && ((t->val>=20 && t->val<=25) || (t->val==ISVAL) ))}
                            /* +, -, -, *, /, mod ,"is" ,"->" are 20, 21, 22, 23 ,24, 25, 251,IFTHENELSEVAL */

IMAGOTERM *imagostacktree[MLVMSTACKTREESIZE];
MLVMTERM *mlvmstacktree[MLVMSTACKTREESIZE];
MLVMNODE *reccreatevar[MLVMMAXVARNUM];
MLVMTERM *mlvmmathstack1[MLVMSTACKTREESIZE];  /* for store the arithmetic expression tree */
MLVMMATHSTACKTERM mlvmmathstack2[MLVMMATHARGUSIZE];            /* for arithmetic calculation */
char tempusermodule[LINE_BUF];                 /* temporary store the output string of usermodule */


int curimagostack=0;
int curmlvmstack=0;
int mlvmclausetype=0;
int mlvmallocn2=0;
int curmlvmmathstack1=0;
int curmlvmmathstack2=0;
int ownmathexp=0;
int owntermcomp=0;
int owndeepcut=0;
int intermediatevalue=-1;
int nondetermgoal=0;     /* for deepcut and neckcut */
int deeplabel=-1;
int emptyusermodule=1;   /* the user module is empty */
int curtempusermodule=0; /* pointer to the current position of tempusermodule */
int colonposition=-1;    /* remember the latest colonposition for IMAGO */
int userdefgoal=0;        /* store how many user defined goal in the clause */
int builtingoal=0;        /* store how many system built-in or math expression in the clause */
int lastgoal=-1;          /* -1: not defined     0: build-in or math       1: user defined */
int firsttermeq=-1;        /* store the first address for = */
int secondtermeq=-1;       /* stoer the second address for = */
IMAGONAMENODE *imagonamehead=NULL;  /* point to the header of the temporary IMAGO name linked-list */
int imagotype=0;
/*FILE *outlsfile;*/          /* write the *.ls file */
char tempfile[100];
int sourcefiletype=-1;     /* 1--imago file,   0--standard prolog file */
int ifthenelseno=1;         /* the predicate/arity.if.*  */
int ownifthenelse=0;        /* 0-have no ifthenesle,   otherwise 1  */
MLVMTERM *ifcomparison=NULL; /* remember the comparison MLVMTERM before the "->" */
MLVMTERM *beforeifthenelse=NULL; /* remember the MLVMTERM immediately before the "->" */
char *curproname;
char curargnum;
char tempoutstr[LINE_BUF];          /* record the output string temporarily */
char headname[MAX_NAME_LEN];     /* record the headname of a clause which contain -> */
char varname[50][MAX_NAME_LEN];   /* store all the var name which need in translation of -> */
int varnamenum=0;               /* store the current number of varname */
int ifthenelsenum=0;           /* the NO. for the sequence of clause name */
char switch4label[4][MAX_NAME_LEN];           /* [0]-VAR, [1]-CON, [2]-LIS, [3]-STR; 0-fail, 1-lable */
int switch4labelpos[4];        /* the mapping of label and it position */
char* hashentry[MAX_HASHENTRY_NUM];
int switchargno=0;               /* store the no of argument used to do switch */
int opendoublequote=0;           /* 1: meet with the open double quote "    0: close quote */

char ch1;
char buffer1[LINE_BUF];
char inputfiledir[LINE_BUF];   /* the directory of file parameter */
int COMMENT_NEST1 = 1;
char unread_buf1[LINE_BUF];
int unread_cnt1 = 0;
char line_buf1[LINE_BUF];
int old_len1 = 0, line_pos1 = 0;
int lineno1 = 1;
int havefail=0;
int OPTIMDECISION=0;   /* 0: not decide yet, -1: no lastswitch, hashtable possible,  -2: give up , 1: lastswitch,  2: hashtable */
int totalclausenum=0;    /* the clause number with the same name/arity  */
int optimclausetype[3];
int hashentrynum=0;    /* the hashtable entry number */

int curpos10[10]={0,0,0,0,0,0,0,0,0,0}, curstr=0, needtoprint=0, parennum=0, ifthenelsetail=0, multiif=0, tempmultiif1=0;
char tempstr[LINE_BUF], multiarrow1[LINE_BUF],tempstr10[10][LINE_BUF];
char fileargument[100];     /* the file name argument needed to be compiled */
char MASMFILE[]="masm";               /* store the name of "masm"*/

/* for isatom, isatomic .... */
int ATOMNO=-1,ATOMICNO=-1, FLOATNO=-1, INTEGERNO=-1, NUMBERNO=-1, VARNO=-1, NONVARNO=-1, LISTNO=-1, STRUCTURENO=-1, GROUNDNO=-1, COMPOUNDNO=-1;
int isvarcell=-1;

char mlvmreadchar(FILE *, int);
FILE *my_open();

/*---------------------------------------------------------------------------*/
void print_hashfunctab()
{
	IMAGOTERM *tt, *ttex;
	int i, *tmp1;
	int ii, jj;
	struct HASH_TAB_NODE *ppp;
	IMAGOTERM *ttt, *tttnext;
	HASH_CLAUSE_NODE_POINTER hhpp;
	CLAUSE *rrrr;
	char *builtinname[500];
    int builtinargunum[500];

	for(ii=0; ii<500; ii++)
	{
		builtinargunum[ii]=-1;
	}

	ii=0;
	while(ii<1999 )
	{
		ppp=hash_name_tab[ii];
		if(ppp!=NULL )
		{
			printf("  hash_name_tab[%d]->s=%s,  ->val=%d, ->argunum=%d\n", ii, ppp->s, ppp->val, ppp->argunum);
			builtinname[ppp->val]=ppp->s;
			builtinargunum[ppp->val]=ppp->argunum;
			ttt=ppp->t;
			while(ttt!=NULL)
			{
				printf(" imagoterm of hash_name_tab[%d]->t->val=%d, ->argunum=%d\n", ii,ttt->val, ttt->argunum);
				hhpp=hash_imago_table[ttt->val+1];
				if(hhpp!=NULL)
				{
					printf("hash_imago_table[%d]->n_clauses=%d, ->name=%d\n", ttt->val+1, hhpp->n_clauses, hhpp->name);
					if(hhpp->first_clause!=NULL)
					printf("first_clause->id=%d, ->t->str=%s, ->startcode=%d, endcode=%d\n",
					         hhpp->first_clause->id, hhpp->first_clause->t->str, hhpp->first_clause->startcode, hhpp->first_clause->endcode);
					if(hhpp->last_compiled_clause!=NULL)
					printf("last_compiled_clause->id=%d, ->t->str=%s, ->startcode=%d, endcode=%d\n",
					         hhpp->last_compiled_clause->id, hhpp->last_compiled_clause->t->str, hhpp->last_compiled_clause->startcode,
							 hhpp->last_compiled_clause->endcode);

				}
				ttt=ttt->next;
			}
		}

		ii++;
	}
	printf("--------------------builtin table --------------------------------\n");
	printf("  the NO.                 name/arity\n");
	for(ii=0; ii<251; ii++)
	{
		if(builtinargunum[ii]!=-1)
		printf(" %d                 %s/%d\n", ii, builtinname[ii], builtinargunum[ii]);
	}
	printf("--------------------end of builtin table -------------------------\n");
}

void print_hashvartab()
{
	IMAGOTERM *tt, *ttex;
	int i, *tmp1;
	int ii, jj;
	struct HASH_TAB_NODE *ppp;
	IMAGOTERM *ttt, *tttnext;
	HASH_CLAUSE_NODE_POINTER hhpp;
	CLAUSE *rrrr;
	ii=0;
	while(ii<1999 )
	{
		ppp=hash_var_tab[ii];
		if(ppp!=NULL )
		{
			printf("  hash_var_tab[%d]->s=%s, ->special=%d, ->quick_hash=%d,  ->val=%d, ->argunum=%d\n", ii,   ppp->s,ppp->special,ppp->quick_hash, ppp->val, ppp->argunum);
			ttt=ppp->t;
			while(ttt!=NULL)
			{
				printf(" hash_var_tab[%d]->t->val=%d, ->argunum=%d\n", ii,ttt->val, ttt->argunum);
				hhpp=hash_imago_table[ttt->val+1];
				if(hhpp!=NULL)
				{
					printf("hash_imago_table[%d]->n_clauses=%d, ->name=%d\n", ttt->val+1, hhpp->n_clauses, hhpp->name);
					if(hhpp->first_clause!=NULL)
					printf("first_clause->id=%d, ->t->str=%s, ->startcode=%d, endcode=%d\n",
					         hhpp->first_clause->id, hhpp->first_clause->t->str, hhpp->first_clause->startcode, hhpp->first_clause->endcode);
					if(hhpp->last_compiled_clause!=NULL)
					printf("last_compiled_clause->id=%d, ->t->str=%s, ->startcode=%d, endcode=%d\n",
					         hhpp->last_compiled_clause->id, hhpp->last_compiled_clause->t->str, hhpp->last_compiled_clause->startcode,
							 hhpp->last_compiled_clause->endcode);
				}
				ttt=ttt->next;
			}
		}
		ii++;
	}
}

void printtree(IMAGOTERM *rootterm, int rootfirstnext, IMAGOTERM *parentnode) /* rootfirstnext=0: root,  1: first kids,  2: next brother */
{
	static int ii=0;
	if(rootterm!=NULL)
	{
			if(parentnode==NULL)
			fprintf(outlsfile,"addr=%d,  term %d : ty=%d, create_var=%d, extra_flag=%d, argunum=%d, val=%d, str=%s , leftorright=%d\n",
						(int) rootterm, ii, rootterm->ty, rootterm->create_var, rootterm->extra_flag, rootterm->argunum,
							 rootterm->val, rootterm->str, rootfirstnext);
			else
			fprintf(outlsfile,"addr=%d,  term %d : ty=%d, create_var=%d, extra_flag=%d, argunum=%d, val=%d, str=%s , leftorright=%d, pointednode=%s\n",
									(int) rootterm, ii, rootterm->ty, rootterm->create_var, rootterm->extra_flag, rootterm->argunum,
							 rootterm->val, rootterm->str, rootfirstnext, parentnode->str);

		ii++;
		if(rootterm->first!=NULL){printtree(rootterm->first, 1, rootterm);	ii++;}
		if(rootterm->next!=NULL) {	printtree(rootterm->next, 2, rootterm);	ii++;}
	}
}

void printmlvmtree(MLVMTERM *mlvmrootterm)
{
	static int iii=0;
	if(mlvmrootterm!=NULL)
	{
			fprintf(outlsfile, "addr=%d,  term %d : type=%d, level=%d, index=%d, annot=%d,argunum=%d, node->first=%d, str=%s, rval=%f \n",
						(int) mlvmrootterm, iii, mlvmrootterm->type, mlvmrootterm->level,
						mlvmrootterm->index, mlvmrootterm->annot,mlvmrootterm->argunum, mlvmrootterm->node->first,
						     mlvmrootterm->node->name, mlvmrootterm->rval);
		iii++;
		if(mlvmrootterm->kids!=NULL){printmlvmtree(mlvmrootterm->kids);	iii++;}
		if(mlvmrootterm->next!=NULL){printmlvmtree(mlvmrootterm->next);	iii++;}
	}
}

MLVMTERM *allocateterm()
{
	MLVMTERM *tempimagoterm;
	if((tempimagoterm=(MLVMTERM *)malloc(sizeof(MLVMTERM)))==NULL)
	{
		printf("Not enough memory for MLVMTERM \n");
		exit(-1);
	}
	else if((tempimagoterm->node=(MLVMNODE *)malloc(sizeof(MLVMNODE)))==NULL)
		 {
		 	printf("Not enough memory for MLVMNODE\n");
			exit(-1);
		 }
		 else return tempimagoterm;
}

MLVMCLAUSELINK *allocateclauselink()
{
	MLVMCLAUSELINK *tempimagoterm;
	if((tempimagoterm=(MLVMCLAUSELINK *)malloc(sizeof(MLVMCLAUSELINK)))==NULL)
	{
		printf("Not enough memory for MLVMCLAUSELINK \n");
		exit(-1);
	}
	else return tempimagoterm;
}

MLVMTERM *popmlvm()
{
	if(curmlvmstack==0)	return NULL;
	else return mlvmstacktree[--curmlvmstack];
}

MLVMTERM *popmlvmmath1()
{
	if(curmlvmmathstack1==0) return NULL;
	else return mlvmmathstack1[--curmlvmmathstack1];
}

MLVMMATHSTACKTERM *popmlvmmath2()
{
	if(curmlvmmathstack2==0) return NULL;
	else
	{
		curmlvmmathstack2--;
		return (&mlvmmathstack2[curmlvmmathstack2]);
	}
}


IMAGOTERM *pop()
{
	if(curimagostack==0)	return NULL;
	else return imagostacktree[--curimagostack];
}

void pushmlvm(MLVMTERM *inputmlvmterm)
{
	if(curmlvmstack==MLVMSTACKTREESIZE)
	{
		printf("No stack space for pushmlvm\n");
		exit(-1);
	}
	else mlvmstacktree[curmlvmstack++]=inputmlvmterm;
}

void push(IMAGOTERM *inputclimagoterm)
{
	if(curimagostack==MLVMSTACKTREESIZE)
	{
		printf("No stack space for push\n");
		exit(-1);
	}
	else imagostacktree[curimagostack++]=inputclimagoterm;
}

void pushmlvmmath1(MLVMTERM *inputmlvmterm)
{
	if(curmlvmmathstack1==MLVMSTACKTREESIZE)
	{
		printf("No stack space for pushmlvm\n");
		exit(-1);
	}
	else mlvmmathstack1[curmlvmmathstack1++]=inputmlvmterm;
}

void pushmlvmmath2(int type,int intval,double floatval,int address)
{
	if(curmlvmmathstack2==MLVMMATHARGUSIZE)
	{
		printf("No stack space for pushmlvm\n");
		exit(-1);
	}
	else
	{
		mlvmmathstack2[curmlvmmathstack2].type=type;
		mlvmmathstack2[curmlvmmathstack2].intval=intval;
		mlvmmathstack2[curmlvmmathstack2].floatval=floatval;
		mlvmmathstack2[curmlvmmathstack2].address=address;
		curmlvmmathstack2++;
	}
}

void printmlvmmath1()
{
	int ii;
	for(ii=curmlvmmathstack1; ii>=0 ; ii-- )
		printf("mlvmmathstack1[%d]=%s\n",ii,mlvmmathstack1[ii]->node->name );
}

clausetype(IMAGOTERM *rootterm)
{
	IMAGOTERM *t;
	t=rootterm;
	if(t->ty==FUNC && t->val==RULEIFVAL)      /* get the type of clause */
	{
		if(t->first->next->ty==FUNC && (t->first->next->val==COMMAVAL || t->first->next->val==IFTHENELSEVAL ))
		{
			/********************* MLVMSPENDCLAUSE will be considered later *******************/
			mlvmclausetype=MLVMUSERENDCLAUSE;
		}
		else mlvmclausetype=MLVMCHAIN;
	}
	else mlvmclausetype=MLVMFACT;
}

/*****************************************************************************************
* what type contributes to mlvmallocn2:  STR, REAL, LIST, NEW_VAR and MATH_EXPRESSION   *
****************************************************************************************/

int isinstruction(IMAGOTERM *inputterm)
{
	if(inputterm->argunum==1)
	{
		if(!strcmp(inputterm->str, "atom")) { printf("***isatom******\n");return (ATOMNO=(int)inputterm->val);}
		else if(!strcmp(inputterm->str, "atomic")  ) return (ATOMICNO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "float")  ) return (FLOATNO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "integer")  ) return (INTEGERNO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "number")  ) return (NUMBERNO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "var")  ) return (VARNO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "nonvar")  ) return (NONVARNO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "list")  ) return (LISTNO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "structure")  ) return (STRUCTURENO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "ground")  ) return (GROUNDNO=(int)inputterm->val);
		else if(!strcmp(inputterm->str, "compound")  ) return (COMPOUNDNO=(int)inputterm->val);
		else return 0;
	} else return 0;
}

int istest(int inputint)
{
	if(inputint==ATOMNO) {return 1; }
	else if(inputint==ATOMICNO) {return 1; }
	else if(inputint==FLOATNO) {return 1; }
	else if(inputint==INTEGERNO) {return 1; }
	else if(inputint==NUMBERNO) {return 1; }
	else if(inputint==VARNO) {return 1; }
	else if(inputint==NONVARNO) {return 1; }
	else if(inputint==LISTNO) {return 1; }
	else if(inputint==STRUCTURENO) {return 1; }
	else if(inputint==GROUNDNO) {return 1; }
	else if(inputint==COMPOUNDNO) {return 1; }
	else return 0;
}

int isoutput(FILE *inputfile, int inputint)
{
	if(inputint==ATOMNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isatom", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==ATOMICNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isatomic", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==FLOATNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isfloat", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==INTEGERNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isinteger", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==NUMBERNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isnumber", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==VARNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isvar", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==NONVARNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isnonvar", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==LISTNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","islist", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==STRUCTURENO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isstr", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==GROUNDNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","isground", isvarcell );isvarcell=-1;return 1; }
	else if(inputint==COMPOUNDNO) {fprintf(inputfile, "%-20s%-20s%d fail\n", "","iscompound", isvarcell );isvarcell=-1;return 1; }
	else return 0;
}


MLVMTERM *mlvmtermtree(IMAGOTERM *rootterm) /* from the first imagoterm of the fact */
{                                       /* create the mlvm term tree from the CLP term tree */
	MLVMTERM *tempimagoterm, *headterm;
	IMAGOTERM *t;
	int tlevel, cget, rlab, wlab, enterbody, firsttime, entermathexp, alreadyifcomp,tempint;
		tlevel = -1;       /* the first mlvmterm have not level value */
		alreadyifcomp=0;
		mlvmallocn2 = wlab = rlab = cget =enterbody = ownmathexp=owntermcomp=owndeepcut =firsttime = entermathexp = ownifthenelse=userdefgoal = 0;
		builtingoal=tempint=0;
		nondetermgoal=0;
		intermediatevalue=deeplabel= lastgoal =-1;
		ifcomparison= beforeifthenelse=NULL;
		headterm=tempimagoterm=allocateterm();
		if(mlvmclausetype==MLVMUSERENDCLAUSE || mlvmclausetype==MLVMCHAIN) t=rootterm->first;                  /* skip the ":-"  */
		else t=rootterm;
		headterm->level=-1;
		do
		{
			tlevel=tempimagoterm->level;
			tempimagoterm->argunum=t->argunum;
			tempimagoterm->index=-1;    /* initialize them */
			tempimagoterm->annot=-2;

			if((t->ty==FUNC && t->val==COMMAVAL) || (tempimagoterm->level==-1 && firsttime==1) )
			                           /* encounter ",", set tlevel to determine allocate for create var or not */
									   /* the second condition is for the MLVMCHAIN, because CHAINCALL has not COMMAVAL */
			{
				tlevel=-1;
				enterbody=1;
			}
			if(tempimagoterm->level==-1)
			{
				firsttime++;            /* if encounter the second MLVMTERM with level==-1, that means enter the first goal
			                              of the clause */
			}
			if(t->ty==FUNC && t->argunum!=0) /* STR or LIS*/
			{
				isinstruction(t);
				if(t->val==COMMAVAL ) { tempimagoterm->type=SYMCOMMAVAL;entermathexp=0;}
				else if(t->val==RULEIFVAL) { tempimagoterm->type=SYMRULEIFVAL;entermathexp=0;}
				else if(t->val==DOTVAL) {
										if(t->first->create_var==1 && t->first->next->create_var==1)
											tempimagoterm->type=MLVMLVS;
										else tempimagoterm->type=MLVMLIS;
										entermathexp=0;
					                 }      /* DOTVAL represent list in MLVM */
				else if(t->ty==FUNC && ((t->val>=20 && t->val<=25) || (t->val==ISVAL)))
				      {
					  		tempimagoterm->type=MLVMOPERATOR;
							ownmathexp=1;
							tempimagoterm->rval=t->val;
							entermathexp=1;
							builtingoal++;
							lastgoal=0;
					  }
				else if(t->ty==FUNC && (t->val>=40 && t->val<=45)) /* only for arithmetic expression */
					  {
							tempimagoterm->type=MLVMCOMPARISON;
							ownmathexp=1;
							tempimagoterm->rval=t->val;
							entermathexp=1;
							builtingoal++;
							lastgoal=0;
							if(ownifthenelse==1 && alreadyifcomp==0)
								ifcomparison=tempimagoterm;
					  }
				/*else if(t->ty==FUNC && (t->val==UEQVAL || t->val==UNEVAL))*/ /* = and /= */
				else if(t->ty==FUNC && (t->val>=66 && t->val<=73))
					  {
							tempimagoterm->type=MLVMTERMCOMPARISON;
							builtingoal++;
							tempimagoterm->rval=t->val;
							lastgoal=0;
					  }
				else if(t->val==IFTHENELSEVAL)        /* -> */
						{
							tempimagoterm->type=MLVMIFTHENELSE;
							ownifthenelse=1;
							tempimagoterm->rval=t->val;
						}
				else { tempimagoterm->type=STR; tempimagoterm->rval=t->val; entermathexp=0;
						if(t->val>250 && enterbody==1 && tempimagoterm->level==0 ) {userdefgoal++; lastgoal=1; }
						if(t->val<=250 && enterbody==1 && tempimagoterm->level==0 ) {builtingoal++; lastgoal=0; }
						if(hash_imago_table[t->val+1]!=NULL)           /* may be a bug  */
						if(hash_imago_table[t->val+1]->n_clauses>1 && enterbody==1) nondetermgoal=1;

					}
				if(enterbody==0)
				{
					if(tempimagoterm->level>=0 && t->val!=COMMAVAL && t->val!=RULEIFVAL )
				                           /* this will exclude the functor itself, and ":-", ","  */
					{
						if(t->val!=DOTVAL) mlvmallocn2=1+mlvmallocn2+t->argunum; /* the functor self plus number of argument */
						else mlvmallocn2=mlvmallocn2+t->argunum;
					}
				}
				else
				{
					if(mlvmclausetype==MLVMUSERENDCLAUSE)
					{
						if(tempimagoterm->level>=1 && t->val!=COMMAVAL && t->val!=RULEIFVAL && t->val!=IFTHENELSEVAL && ((t->val<20 || t->val>25) && (t->val!=ISVAL) ) )
									                           /* this will exclude the functor itself, and ":-", "," and math expression , IFTHENELSEVAL represent -> */
						{
							/* the functor self plus number of argument */
							/* DOTVAL means LIST type */
							/*if(t->val!=DOTVAL) mlvmallocn2=1+mlvmallocn2+t->argunum;
							else mlvmallocn2=1+mlvmallocn2+t->argunum;*/
							if(t->val!=DOTVAL)
							{
								if(tempimagoterm->level==1) mlvmallocn2=2+mlvmallocn2+t->argunum;
								else mlvmallocn2=1+mlvmallocn2+t->argunum;
							}
							else
							{
								if(tempimagoterm->level==1) mlvmallocn2=1+mlvmallocn2+t->argunum;
								else mlvmallocn2=mlvmallocn2+t->argunum;
							}
						}
					}
					if(mlvmclausetype==MLVMCHAIN)
					{
						if(tempimagoterm->level>=0 && t->val!=COMMAVAL && t->val!=RULEIFVAL && ((t->val<20 || t->val>25) && (t->val!=ISVAL)))
						{
							/* the functor self plus number of argument */
							/*if(t->val!=DOTVAL)  mlvmallocn2=1+mlvmallocn2+t->argunum;
							else mlvmallocn2=1+mlvmallocn2+t->argunum;*/
							if(t->val!=DOTVAL)
							{
								if(tempimagoterm->level==0) mlvmallocn2=2+mlvmallocn2+t->argunum;
								else mlvmallocn2=1+mlvmallocn2+t->argunum;
							}
							else
							{
								if(tempimagoterm->level==0) mlvmallocn2=1+mlvmallocn2+t->argunum;
								else mlvmallocn2=mlvmallocn2+t->argunum;
							}
						}
					}

				}
			}
			else if(t->ty==FUNC && t->argunum==0 ) /* ATOM or CUT or goals own such style  name/0 */
			{
				if(t->val==CUTVAL )
				{
					if(nondetermgoal==1) {tempimagoterm->type=MLVMDEEPCUT; owndeepcut=1;}/*CUTVAL==113 */
					else tempimagoterm->type=MLVMNECKCUT;
					lastgoal=0;
				}
				else if(t->val==NILLISTVAL) { tempimagoterm->type=MLVMNIL; tempimagoterm->rval=t->val; lastgoal=0;}
				/* ATOM in the term comparsion and unifiable , need to be set */
				else{
						tempimagoterm->type=ATOM; tempimagoterm->rval=t->val ;
						if(owntermcomp>0)
						{
							mlvmallocn2++;   /* allocate memory for ATOM in term comparison */
							tempimagoterm->type=MLVMATOMCOMP;
						}
						else if((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && tempimagoterm->level==0 && t->val > 251) {userdefgoal++;lastgoal=1; }
						else if((mlvmclausetype==MLVMCHAIN) && tempimagoterm->level==-1 && t->val>251) {userdefgoal++;lastgoal=1; }
						else if((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && tempimagoterm->level==0 && t->val < 250) {builtingoal++;lastgoal=0; }
						else if((mlvmclausetype==MLVMCHAIN) && tempimagoterm->level==-1 && t->val<250) {builtingoal++;lastgoal=0; }
					}
			}
			else if(t->ty==VAR && t->create_var==3)
			{
				tempimagoterm->type=VOIDVAR;
			}
			else if(t->ty==REAL)
			{
				if((t->rval - (int) t->rval) >0)  /* is INT or REAL */
				tempimagoterm->type=REAL;
				else tempimagoterm->type=MLVMINT;
				tempimagoterm->rval=t->rval;
				if(tempimagoterm->type==REAL)
				{
					if(enterbody==0)        /* head of clause */
					{
						if(tempimagoterm->level>1) mlvmallocn2++; /* only the REAL in level>1 need to allocate */
					}
					else if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
					{
						if(tempimagoterm->level==1 && entermathexp== 0) mlvmallocn2++;
						   /* only the float in goal in level=1 need additional cell,others are calculated in STR
						     arguments , exception is the arithmetic float need not allocation, just setfloat
							 to the temporary cell during calculation, the REAL in math expression need not allocated */
					}
					else if(mlvmclausetype==MLVMCHAIN)
					{
						if(tempimagoterm->level==0 && entermathexp== 0) mlvmallocn2++;
					}
				}
			}
			else {tempimagoterm->type=t->ty; }

			if(ownifthenelse==1 && alreadyifcomp==0 && t->val!=SIMECOLONVAL)
			{
				beforeifthenelse=tempimagoterm;
			}
			if(t->val==SIMECOLONVAL) alreadyifcomp=1;  /* SIMECOLONVAL = ";" */
			if(t->create_var==2)   /* used variable */
			{
				free(tempimagoterm->node);
				tempimagoterm->node=reccreatevar[t->val]; /* point to previous MLVMNODE */

			}
			else if(t->create_var==1)   /* first occurence variable */
			{
				tempimagoterm->node->name=t->str;
				tempimagoterm->node->first=-1;  /* the initial value is -1 */
				reccreatevar[t->val]=tempimagoterm->node;
				/* if this var is not in the STR or LIS , need to allocate */
				/*if(tlevel==1 && enterbody==1)
				mlvmallocn2++;*/
				if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
				{
					if(tlevel==1 && enterbody== 1) mlvmallocn2++;
				}
				else if(mlvmclausetype==MLVMCHAIN)
				{
					if(tlevel==0 && enterbody== 1) mlvmallocn2++;
				}
			}
			else              /* other than variable  */
			{
				tempimagoterm->node->name=t->str;
				tempimagoterm->node->first=-1;  /* the initial value is -1 */
			}
			if (t->next)
			{
				if(owntermcomp==2)
				{
					owntermcomp=0;
				}
				if(owntermcomp==1)
				{
					owntermcomp=2;
				}

				tempimagoterm->next=allocateterm();
				tempimagoterm->next->level=tlevel;
				pushmlvm(tempimagoterm->next);
				push(t->next);
			}
			else tempimagoterm->next=NULL;
			if (t->first)
			{
				if(owntermcomp==2)
				{
					owntermcomp=0;
				}
				if(tempimagoterm->type==MLVMTERMCOMPARISON)  /* this is for ATOM in termcomparison */
				{
					owntermcomp=1;
				}
				tlevel++;
				tempimagoterm->kids=allocateterm();
				tempimagoterm->kids->level=tlevel;
				pushmlvm(tempimagoterm->kids);
				push(t->first);
			}
			else tempimagoterm->kids=NULL;
		} while ((t = pop()) && (tempimagoterm=popmlvm()));
		if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
		{
			if(ownmathexp==1)
			{
				mlvmallocn2=mlvmallocn2+ACTIVATIONFRAME+4;intermediatevalue=3; /* 4 temporary cells for math calculation */
				if(owndeepcut==1) {mlvmallocn2++; deeplabel=7;}
			}
			else
			{
				mlvmallocn2=mlvmallocn2+ACTIVATIONFRAME;
				if(owndeepcut==1) {mlvmallocn2++; deeplabel=3;}
			}
		}
		else
		{
			if(ownmathexp==1) {mlvmallocn2=mlvmallocn2+4; intermediatevalue=0;} /* 4 temporary cells for math calculation */
		}
		return headterm;
}


assign_cell(MLVMTERM *t, int size)  /* from the outmost functor, old version just set the head of clause */
{                                   /* I need to modify it to apply to body of clause */
     MLVMTERM* sib;
	 int enterbody=0;
	 int firsttime=0;
	 do
	 {
	 	if(t->type==SYMCOMMAVAL || (t->level==-1 && firsttime==1)) enterbody=1;
		if(t->level==-1)
			firsttime++;            /* if encounter the second MLVMTERM with level==-1, that means enter the first goal
				                              of the clause */
		if (t->type == STR || t->type ==MLVMTERMCOMPARISON )  /* include the =/2  */
		{
			/*if(enterbody==0 || (enterbody==1 && t->level>=1)) t->annot = size--;*/
			/*if(enterbody==0 || (enterbody==1 && t->level>=2)) t->annot = size--;*/
			if(enterbody==0) t->annot = size--;
			else
			{
				if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
				{
					if(t->level>=1) t->annot=size--;
				}
				else if(mlvmclausetype==MLVMCHAIN)
				{
					if(t->level>=0) t->annot=size--;
				}
			}

			sib = t->kids;
			while(sib)
				{
					sib->index = size--;
					if(sib->type == VAR)
					{
						if(sib->node->first==-1)  /* first occurence */
						{
							sib->node->first = sib->index;
							sib->annot = sib->index;
						}
						else                      /* used var */
						{
							sib->annot = sib->node->first;
							if(enterbody==1 && sib->level<2) size++;   /* don't need to allocate if not in nested STR */
						}
					}
					else if(sib->type!=STR && sib->type!=MLVMLIS && sib->type!=MLVMLVS && sib->type!=MLVMATOMCOMP)
					/* the ATOM, CON, MLVMINT do not need allocation */
					{
						/*if(enterbody==1 && sib->level<2) size++;*/
						if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
						{
							if(enterbody==1 && sib->level<2) size++;
						}
						else if(mlvmclausetype==MLVMCHAIN)
						{
							if(enterbody==1 && sib->level<1) size++;
						}
					}
					sib = sib->next;
				}/* while */


		}else if(t->type==MLVMLIS || t->type==MLVMLVS) /*else if (t->type == LIS)*/
			{
				t->annot = size;
				/*if(enterbody==0)
				{
					if (t->level == 0) t->type = MLVMLVS;
				}
				*/
				sib = t->kids;
				while (sib)
				{
					sib->index = size--;
					if (sib->type == VAR)
					{
						if(sib->node->first==-1)
						{
							sib->node->first = sib->index;
							sib->annot = sib->index;
						}
						else
						{
							/*t->type = MLVMLIS;    */
							sib->annot = sib->node->first;
						}
					}
					/*else t->type = MLVMLIS; */
					sib = sib->next;
				}
			}

			if (t->type == MLVMOPERATOR || t->type == MLVMCOMPARISON || t->type == MLVMIFCOMPARISON )  /* the create_var need to be allocated, that is all */
			                              /* uneccessary for MLVMCOMPARISON */
					{
						/*if(enterbody==0 || (enterbody==1 && t->level>=1)) t->annot = size--;*/
						if(enterbody==0) t->annot = size--;
						sib = t->kids;
						while(sib)
						{
							if(sib->type==VAR)  /* only apply to VAR */
								sib->index = size--;
							if(sib->type == VAR)
							{
								if(sib->node->first==-1)  /* first occurence */
								{
									sib->node->first = sib->index;
									sib->annot = sib->index;
								}
								else                      /* used var */
								{
									sib->annot = sib->node->first;
									if(enterbody==1 ) size++;   /* don't need to allocate if used var  */
								}
							}
							sib = sib->next;
					}/* while */
				}
				if (t->next) pushmlvm(t->next);
				if (t->kids) pushmlvm(t->kids);
	} while (t = popmlvm());
}

int coding_get(char *proname, int argnum,MLVMTERM* t, int c, int wlab, int inputclausenum)
{
	switch (t->type)
	{
		/*case REF: */
		case VAR:
				if (t->index != t->annot)
				{
					if (c) fprintf(outlsfile, "%-20s%-20s%d\n", "","cgetval",t->annot);       /*[cgetval t->annot]; */
					else fprintf(outlsfile, "%-20s%-20s%d  %d\n","","getval", t->index, t->annot);    /*[getval t->index, t->annot]; */
					return 1;
				}
				else return 0;
		/*case FLP:                  */
		case REAL:
				if (c) fprintf(outlsfile, "%-20s%-20s%f\n","","cgetfloat", t->rval); /*[cgetfloat t->node->name];   */
				else fprintf(outlsfile, "%-20s%-20s%d  %f\n","","getfloat", t->index, t->rval); /*[getfloat t->index, t->node->name];   */
				return 1;
		case MLVMINT:
				if (c) fprintf(outlsfile, "%-20s%-20s%d\n","","cgetint",(int) t->rval); /*[cgetfloat t->node->name];   */
				else fprintf(outlsfile, "%-20s%-20s%d  %d\n", "","getint",t->index, (int) t->rval); /*[getfloat t->index, t->node->name];   */
				return 1;
		/*case CON:       */
		case ATOM:
				if (c) fprintf(outlsfile, "%-20s%-20s '%s'\n","","cgetcon", t->node->name);      /*[cgetcon t->node->name];  */
				else fprintf(outlsfile, "%-20s%-20s%d '%s'\n", "","getcon",t->index, t->node->name); /*[getcon t->index, t->node->name];  */
				return 1;
		case MLVMNIL:
				if (c) fprintf(outlsfile, "%-20s%-20s '%s'\n","","cgetcon", t->node->name);      /*[cgetcon t->node->name];  */
				else fprintf(outlsfile, "%-20s%-20s%d  '%s' \n", "","getcon",t->index, t->node->name); /*[getcon t->index, t->node->name];  */
				return 1;
		case MLVMLIS:
				if(inputclausenum>0)
					if (c) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.%d.w.%d\n","","cgetlist",t->annot, t->level, proname,argnum,inputclausenum, wlab);/* [cgetlist t->annot, t->level, W.wlab]; */
					else fprintf(outlsfile, "%-20s%-20s%d  %d  %d  %s/%d.%d.w.%d\n","","getlist",t->index, t->annot, t->level, proname,argnum,inputclausenum, wlab);
				else
					if (c) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.w.%d\n","","cgetlist",t->annot, t->level, proname,argnum, wlab);/* [cgetlist t->annot, t->level, W.wlab]; */
					else fprintf(outlsfile, "%-20s%-20s%d  %d  %d  %s/%d.w.%d\n","","getlist",t->index, t->annot, t->level, proname,argnum, wlab);
				                               /*[getlist t->index, t->annot, t->level, W.wlab]; */
				return 1;
		case MLVMLVS:
				if (c) fprintf(outlsfile, "%-20s%-20s%d  \n","","cgetvlist",t->annot );/* [cgetlist t->annot, t->level, W.wlab]; */
				else fprintf(outlsfile, "%-20s%-20s%d  %d \n","","getvlist",t->index, t->annot);
						                               /*[getlist t->index, t->annot, t->level, W.wlab]; */
				return 1;

		case STR:
				if(inputclausenum>0)
					if (c) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d  %s/%d.%d.w.%d \n","","cgetstr",
					           t->annot, t->level, t->node->name, t->argunum,proname,argnum,inputclausenum, wlab);
								   /*[cgetstr t->annot, t->level, t->node->name, W.wlab];      */
					else fprintf(outlsfile, "%-20s%-20s%d  %d  %d  %s/%d  %s/%d.%d.w.%d \n","","getstr",
					           t->index, t->annot, t->level, t->node->name,t->argunum, proname,argnum,inputclausenum, wlab);
						   /*[getstr t->index, t->annot, t->level, t->node->name, W.wlab];     */
				else
					if (c) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d  %s/%d.w.%d \n","","cgetstr",
				           t->annot, t->level, t->node->name, t->argunum,proname,argnum, wlab);
						   /*[cgetstr t->annot, t->level, t->node->name, W.wlab];      */
					else fprintf(outlsfile, "%-20s%-20s%d  %d  %d  %s/%d  %s/%d.w.%d \n","","getstr",
				           t->index, t->annot, t->level, t->node->name,t->argunum, proname,argnum,wlab);
						   /*[getstr t->index, t->annot, t->level, t->node->name, W.wlab];     */
				return 1;
		default: return 0;
		}
}

void read_stream(char *proname, int argnum, MLVMTERM* t, int inputclausenum, int inputclausesize, MLVMCLAUSELINK *inclauselink)
{
	int level, cget, rlab, wlab;
	int ii=0;
	level = t->level;
	wlab = rlab = cget = 0;
	if(havefail==0)
	{
		fprintf(outlsfile, "%-20s%-20s\n","fail:","fail");
		havefail=1;
	}
	if(inputclausenum==0)
	{
		fprintf(outlsfile, "\n%-20s%-20s%s/%d\n","","procedure", proname, argnum);
		fprintf(outlsfile, "%s/%d:\n", proname, argnum);   /* the entry label of whole procedure */
		if(OPTIMDECISION==1)
		{
			if(switchargno==argnum) fprintf(outlsfile, "%-20s%-20s%s  %s  %s  %s\n", "","lastswitch",
			switch4label[0], switch4label[1],switch4label[2],switch4label[3]);
			else fprintf(outlsfile, "%-20s%-20s  %d  %s  %s  %s  %s\n", "","switch", switchargno,
			switch4label[0], switch4label[1],switch4label[2],switch4label[3]);
		}
		else if(OPTIMDECISION==2)
		{
			if(switchargno==argnum) fprintf(outlsfile, "%-20s%-20s%s  %s  %s  %s\n", "","lastswitch",
			switch4label[0], switch4label[1],"fail","fail");
			else fprintf(outlsfile, "%-20s%-20s  %d  %s  %s  %s  %s\n", "","switch", switchargno,
			switch4label[0], switch4label[1],"fail","fail");

			fprintf(outlsfile, "%s:\n", switch4label[1]);
			fprintf(outlsfile, "%-20s%s  %s/%d.h\n", "", "lasthash", proname, argnum);
			fprintf(outlsfile, "%s/%d.h:\n", proname, argnum);
			fprintf(outlsfile, "%-20s%s  %d\n", "", "hashtable", hashentrynum);
			while(ii<hashentrynum)
			{
				fprintf(outlsfile, "%-20s'%s'  %s/%d.he.%d\n", "", hashentry[ii], proname, argnum, ii);
				ii++;
			}
			fprintf(outlsfile, "%-20s%s\n", "", "fail");
		}
		/*fprintf(outlsfile, "%-20s", tempoutstr);   */
		if(inputclausesize!=1)
		{
			/*if(inputclausesize==2) fprintf(outlsfile, "trust  %s/%d.1\n", proname, argnum);
			if(inputclausesize>2) fprintf(outlsfile, "%-20s%-20s%s/%d.1\n","","try", proname, argnum);*/
			if(OPTIMDECISION==1 || OPTIMDECISION==2) fprintf(outlsfile, "%s:\n", switch4label[0]);
			if(inputclausesize>=2) fprintf(outlsfile, "%-20s%-20s%d  %s/%d.1\n", "","try",argnum, proname, argnum);
			if(OPTIMDECISION==1) fprintf(outlsfile, "%s:\n", switch4label[switch4labelpos[inputclausenum]]);
			else if(OPTIMDECISION==2) fprintf(outlsfile, "%s/%d.he.%d:\n", proname, argnum, inputclausenum);

		}
	}
	else
	{
		sprintf(tempoutstr, "%s/%d.%d:", proname, argnum, inputclausenum);
		fprintf(outlsfile, "%-20s\n", tempoutstr);
		/*if(inputclausesize-inputclausenum>2) fprintf(outlsfile, "retry  %s/%d.%d\n", proname, argnum, inputclausenum+1);
		if(inputclausesize-inputclausenum==2) fprintf(outlsfile, "trust  %s/%d.%d\n", proname, argnum, inputclausenum+1);*/
		if(inputclausesize-inputclausenum>=2) fprintf(outlsfile, "%-20s%-20s%d  %s/%d.%d\n", "","retry",argnum, proname, argnum, inputclausenum+1);
		if(inputclausesize-inputclausenum==1) fprintf(outlsfile, "%-20s%-20s\n","","trust");
		if(OPTIMDECISION==1) fprintf(outlsfile, "%s:\n", switch4label[switch4labelpos[inputclausenum]]);
		else if(OPTIMDECISION==2) fprintf(outlsfile, "%s/%d.he.%d:\n", proname, argnum, inputclausenum);
	}
	if(mlvmallocn2!=0)
	{
		if(mlvmclausetype==MLVMFACT || mlvmclausetype==MLVMCHAIN ||
		   ((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==0) ||
		   ((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==1 && lastgoal==1) )
		fprintf(outlsfile, "%-20s%-20s%d  %d\n","","allod", argnum, mlvmallocn2);
		if((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && (userdefgoal>1 || (userdefgoal==1 && lastgoal==0)))
		fprintf(outlsfile, "%-20s%-20s%d  %d\n","","alloa", argnum, mlvmallocn2);
	}

	do
	{
		if (t->level < level)
		{
			if(inputclausenum>0)
			{
				fprintf(outlsfile, "%s/%d.%d.r.%d:\n", proname, argnum, inputclausenum,rlab); /* [R.rlab:];     */
				/*fprintf(outlsfile, "%-20s", tempoutstr); */
			}
			else
			{
				fprintf(outlsfile, "%s/%d.r.%d:\n", proname, argnum, rlab); /* [R.rlab:];     */
				/*fprintf(outlsfile, "%-20s", tempoutstr); */
			}
			rlab++;
		}
		cget = coding_get(proname, argnum, t, (level == t->level) && cget, wlab, inputclausenum);
		/**********************************LIS will be considered later ***********************/
		/*if ( t->type == STR) wlab++;   */
		/*if (t->type == MLVMLIS || t->type == MLVMLVS || t->type == STR) wlab++;*/
		if (t->type == MLVMLIS || t->type == STR) wlab++;
		level = t->level;
		if (t->next) pushmlvm(t->next);
		if (t->kids) pushmlvm(t->kids);
	} while (t = popmlvm());
	if(inputclausenum>0)
	{
		fprintf(outlsfile, "%s/%d.%d.r.end:\n", proname, argnum, inputclausenum); /* [R.rlab:];     */
		if(mlvmclausetype==MLVMFACT) fprintf(outlsfile, "%-20s%-20s\n","", "proceed"); /* [R.rlab:];     */
		/*fprintf(outlsfile, "%-20s",tempoutstr); */
	}
	else
	{
		fprintf(outlsfile, "%s/%d.r.end:\n", proname, argnum);
		if(mlvmclausetype==MLVMFACT) fprintf(outlsfile, "%-20s%-20s\n","", "proceed"); /* [R.rlab:];     */
		/*fprintf(outlsfile, "%-20s",tempoutstr);*/
	}

	/*if(mlvmclausetype==MLVMFACT )
	fprintf(outlsfile, "proceed\n");*/
}

void read_stream0(char *proname, int argnum, int inputclausenum, int inputclausesize, MLVMCLAUSELINK *inclauselink)
{
	int level, cget, rlab, wlab;
	int ii=0;
	level = -1;
	wlab = rlab = cget = 0;
	if(havefail==0)
	{
		fprintf(outlsfile, "%-20s%-20s\n","fail:","fail");
		havefail=1;
	}
	if(inputclausenum==0)
	{
		fprintf(outlsfile, "\n%-20s%-20s%s/%d\n","","procedure", proname, argnum);
		fprintf(outlsfile, "%s/%d:\n", proname, argnum);   /* the entry label of whole procedure */
		if(OPTIMDECISION==1)
		{
			if(switchargno==argnum) fprintf(outlsfile, "%-20s%-20s%s  %s  %s  %s\n", "","lastswitch",
			switch4label[0], switch4label[1],switch4label[2],switch4label[3]);
			else fprintf(outlsfile, "%-20s%-20s  %d  %s  %s  %s  %s\n", "","switch", switchargno,
			switch4label[0], switch4label[1],switch4label[2],switch4label[3]);
		}
		else if(OPTIMDECISION==2)
		{
			if(switchargno==argnum) fprintf(outlsfile, "%-20s%-20s%s  %s  %s  %s\n", "","lastswitch",
			switch4label[0], switch4label[1],"fail","fail");
			else fprintf(outlsfile, "%-20s%-20s  %d  %s  %s  %s  %s\n", "","switch", switchargno,
			switch4label[0], switch4label[1],"fail","fail");

			fprintf(outlsfile, "%s:\n", switch4label[1]);
			fprintf(outlsfile, "%-20s%s  %s/%d.h\n", "", "lasthash", proname, argnum);
			fprintf(outlsfile, "%s/%d.h:\n", proname, argnum);
			fprintf(outlsfile, "%-20s%s  %d\n", "", "hashtable", hashentrynum);
			while(ii<hashentrynum)
			{
				fprintf(outlsfile, "%-20s'%s'  %s/%d.he.%d\n", "", hashentry[ii], proname, argnum, ii);
				ii++;
			}
			fprintf(outlsfile, "%-20s%s\n", "", "fail");
		}
		/*fprintf(outlsfile, "%-20s", tempoutstr);   */
		if(inputclausesize!=1)
		{
			if(OPTIMDECISION==1 || OPTIMDECISION==2) fprintf(outlsfile, "%s:\n", switch4label[0]);
			if(inputclausesize>=2) fprintf(outlsfile, "%-20s%-20s%d  %s/%d.1\n", "","try",argnum, proname, argnum);
			if(OPTIMDECISION==1) fprintf(outlsfile, "%s:\n", switch4label[switch4labelpos[inputclausenum]]);
			else if(OPTIMDECISION==2) fprintf(outlsfile, "%s/%d.he.%d:\n", proname, argnum, inputclausenum);

		}
	}
	else
	{
		sprintf(tempoutstr, "%s/%d.%d:", proname, argnum, inputclausenum);
		fprintf(outlsfile, "%-20s\n", tempoutstr);
		/*if(inputclausesize-inputclausenum>2) fprintf(outlsfile, "retry  %s/%d.%d\n", proname, argnum, inputclausenum+1);
		if(inputclausesize-inputclausenum==2) fprintf(outlsfile, "trust  %s/%d.%d\n", proname, argnum, inputclausenum+1);*/
		if(inputclausesize-inputclausenum>=2) fprintf(outlsfile, "%-20s%-20s%d  %s/%d.%d\n", "","retry",argnum, proname, argnum, inputclausenum+1);
		if(inputclausesize-inputclausenum==1) fprintf(outlsfile, "%-20s%-20s\n","","trust");
		if(OPTIMDECISION==1) fprintf(outlsfile, "%s:\n", switch4label[switch4labelpos[inputclausenum]]);
		else if(OPTIMDECISION==2) fprintf(outlsfile, "%s/%d.he.%d:\n", proname, argnum, inputclausenum);
	}
	if(mlvmallocn2!=0)
	{
		if(mlvmclausetype==MLVMFACT || mlvmclausetype==MLVMCHAIN ||
		   ((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==0) ||
		   ((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==1 && lastgoal==1) )
		fprintf(outlsfile, "%-20s%-20s%d  %d\n","","allod", argnum, mlvmallocn2);
		if((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && (userdefgoal>1 || (userdefgoal==1 && lastgoal==0)))
		fprintf(outlsfile, "%-20s%-20s%d  %d\n","","alloa", argnum, mlvmallocn2);
	}

	if(inputclausenum>0)
	{
		fprintf(outlsfile, "%s/%d.%d.r.end:\n", proname, argnum, inputclausenum); /* [R.rlab:];     */
		if(mlvmclausetype==MLVMFACT) fprintf(outlsfile, "%-20s%-20s\n","", "proceed"); /* [R.rlab:];     */
		/*fprintf(outlsfile, "%-20s",tempoutstr); */
	}
	else
	{
		fprintf(outlsfile, "%s/%d.r.end:\n", proname, argnum);
		if(mlvmclausetype==MLVMFACT) fprintf(outlsfile, "%-20s%-20s\n","", "proceed"); /* [R.rlab:];     */
		/*fprintf(outlsfile, "%-20s",tempoutstr);*/
	}

	/*if(mlvmclausetype==MLVMFACT )
	fprintf(outlsfile, "proceed\n");*/
}



void coding_set(MLVMTERM* t)
{
    switch (t->type){
            /*case REF: */
			case VAR:
                      if (t->index != t->annot) fprintf(outlsfile, "%-20s%-20s%d \n","","csetval", t->annot); /*[csetval t->annot]; */
                      else fprintf(outlsfile, "%-20s%-20s\n","","csetvar"); /*[csetvar ]; */
                      return ;
			case VOIDVAR:
                      fprintf(outlsfile, "%-20s%-20s\n","","csetvar"); /*[csetvar ]; */
                      return ;
            /*case CON: */
			case ATOM:
                      fprintf(outlsfile, "%-20s%-20s'%s'\n", "","csetcon",t->node->name);   /*[csetcon t->node->name]; */
                      return ;
			case MLVMNIL:
                      fprintf(outlsfile, "%-20s%-20s'%s'\n", "","csetcon",t->node->name);   /*[csetcon t->node->name]; */
                      return ;
		/**************************need to be further considered ***************/
            /*case FLP: */
			case REAL:
            		  fprintf(outlsfile, "%-20s%-20s%f\n", "","csetfloat",t->rval);/*          [csetfloat t->node->name]; */
                      return ;
			case MLVMINT:
			          fprintf(outlsfile, "%-20s%-20s%d\n", "","csetint",(int) t->rval);/*          [csetfloat t->node->name]; */
                      return ;
            case MLVMLIS:
                      fprintf(outlsfile, "%-20s%-20s%d\n","","csetlist", t->annot); /*[csetlist t->annot]; */
                      return ;
            case MLVMLVS:
			          fprintf(outlsfile, "%-20s%-20s%d\n","","csetlist", t->annot); /*[csetlist t->annot]; */
                      return ;

            case STR:
                      fprintf(outlsfile, "%-20s%-20s%d\n", "","csetstr",t->annot); /*[csetstr t->annot]; */
                      return ;
        }
}

void write_stream(char *proname, int argnum,MLVMTERM* t, int inputclausenum, int inputclausesize)
{
    int level, wlab, rlab, pending, i, j, before;
	int lab[10];
    MLVMTERM* sib;
    rlab = wlab = pending= 0;
    level = t->level;
	for ( i = 0; i < 10; i ++) lab[i] = -1;
    do {
              if (level > t->level)
			   {
	    			pending = 1;
	    			for ( ; level > t->level; level--)
					{
	    				lab[level-1] = rlab;
					}
               		rlab++;
               }
	           level = t->level;
	          if (t->type == STR)
			  {
	 			if (pending)
					{
						pending = 0;
						i = 0;
						before = 1;
						do
						{
					    	fprintf(outlsfile, "%-20s%-20s","","branch");
						    for (j = 0;  j < 4; j++)
							{
								if (lab[i] != -1)
								{
									if(inputclausenum>0) fprintf(outlsfile, " %s/%d.%d.r.%d ", proname, argnum,inputclausenum,lab[i]);
									else fprintf(outlsfile, " %s/%d.r.%d ", proname, argnum,lab[i]);
									before = 0;
								}
								else
								{
									if (before)
									{
										if(inputclausenum>0) fprintf(outlsfile, " %s/%d.%d.w.%d ", proname, argnum,inputclausenum,wlab);
										else fprintf(outlsfile, " %s/%d.w.%d ", proname, argnum,wlab);

									}
									else
									{
									  	if (j == 3) fprintf(outlsfile, "  fail");
										else	fprintf(outlsfile, "  fail");
				  					}
								}
								i++;
						    }
			 			    fprintf(outlsfile, "\n");
			  			} while (lab[i] != -1);
			 			for (  i = 0; i < 10; i ++) lab[i] = -1;
					}
					if(inputclausenum>0)
					{
						fprintf(outlsfile, "%s/%d.%d.w.%d:\n", proname, argnum,inputclausenum,wlab++);
						/*fprintf(outlsfile, "%-20s", tempoutstr);*/
						fprintf(outlsfile, "%-20s%-20s","","csetfun");
						fprintf(outlsfile, "%s/%d\n",  t->node->name, t->argunum);
					}
					else
					{
						fprintf(outlsfile, "%s/%d.w.%d:\n", proname, argnum,wlab++);
						/*fprintf(outlsfile, "%-20s", tempoutstr);*/
						fprintf(outlsfile, "%-20s%-20s","","csetfun");
						fprintf(outlsfile, "%s/%d\n",  t->node->name, t->argunum);
					}
					sib = t->kids;
					while (sib)
					{
						coding_set(sib);
						sib = sib->next;
					}
				}

			/********************LIS need to be further considered  *****************/
            /*else if (t->type == MLVMLIS ||t->type == MLVMLVS )*/
            else if (t->type == MLVMLIS )
			     {
	 			if (pending)
					{
						pending = 0;
						i = 0;
						before = 1;
						do
						{
					    	fprintf(outlsfile, "%-20s%-20s","","branch");
						    for (j = 0;  j < 4; j++)
							{
								if (lab[i] != -1)
								{
									if(inputclausenum>0) fprintf(outlsfile, "%s/%d.%d.r.%d  ", proname, argnum,inputclausenum,lab[i]);
									else fprintf(outlsfile, "%s/%d.r.%d  ", proname, argnum,lab[i]);
									before = 0;
								}
								else
								{
									if (before)
									{
										if(inputclausenum>0) fprintf(outlsfile, "%s/%d.%d.w.%d  ", proname, argnum,inputclausenum,wlab);
										else fprintf(outlsfile, "%s/%d.w.%d  ", proname, argnum,wlab);

									}
									else
									{
									  	if (j == 3) fprintf(outlsfile, "fail  ");
										else	fprintf(outlsfile, "fail  ");
				  					}
								}
								i++;
						    }
			 			    fprintf(outlsfile, "\n");
			  			} while (lab[i] != -1);
			 			for (  i = 0; i < 10; i ++) lab[i] = -1;
					}
					if(inputclausenum>0)
					{
						fprintf(outlsfile, "%s/%d.%d.w.%d:\n", proname, argnum,inputclausenum,wlab++);

					}
					else
					{
						fprintf(outlsfile, "%s/%d.w.%d:\n", proname, argnum,wlab++);
					}
                    sib = t->kids;
                    while (sib)
					{
                       coding_set(sib);
                       sib = sib->next;
                    }
/*

					sib = t->kids;
					while (sib)
					{
						coding_set(sib);
						sib = sib->next;
					}

					 if(inputclausenum>0)
					 {
						 fprintf(outlsfile, "%s/%d.%d.w.%d:\n", proname, argnum,inputclausenum, wlab);

					 }
					 else
					 {
						 fprintf(outlsfile, "%s/%d.w.%d:\n", proname, argnum,wlab);

					 }
                    wlab++;
                    sib = t->kids;
                    pending=1;
                    while (sib)
					{
                       coding_set(sib);
                       sib = sib->next;
                    }
*/


                  }
           else if (t->type == MLVMLVS ) pending=0; /* for getvlist donot need write stream and branch*/
           if (t->next) pushmlvm(t->next);
           if (t->kids && t->type!= MLVMLVS) pushmlvm(t->kids);
		   /*if (t->kids) pushmlvm(t->kids);*/
     } while (t = popmlvm());
	 /*if (pending){     */
	 if (pending && wlab!=0){
	 			pending = 0;
	 			i = 0;
	 			before = 1;
	 			do {
	 			    fprintf(outlsfile, "%-20s%-20s","","branch");
	 			    for (j = 0;  j < 4; j++){
	 				if (lab[i] != -1){
						if(inputclausenum>0) fprintf(outlsfile, "%s/%d.%d.r.%d  ", proname, argnum,inputclausenum,lab[i]);
						else fprintf(outlsfile, "%s/%d.r.%d  ", proname, argnum,lab[i]);
	 					before = 0;
	 				}
	 				else {
	 					if (before){
							if(inputclausenum>0) fprintf(outlsfile, " %s/%d.%d.r.end  ",proname, argnum, inputclausenum);
							else fprintf(outlsfile, " %s/%d.r.end  ",proname, argnum);
	 					}
	  					else {
	 					  	if (j == 3) fprintf(outlsfile, "fail  ");
	 						else	fprintf(outlsfile, "fail  ");
	    					}
	 				}
	 				i++;
	 			    }
	  			    fprintf(outlsfile, "\n");
	   			} while (lab[i] != -1);
	  			for (  i = 0; i < 10; i ++) lab[i] = -1;
	 		}
	  		/*else if(wlab!=0) fprintf(outlsfile, "%-20s%-20s%s/%d.r.end  fail  fail  fail\n", "","branch",proname, argnum);*/
	  		else if(wlab!=0)
	  		{
	  			if(inputclausenum>0) fprintf(outlsfile, "%-20s%-20s%s/%d.%d.r.end  fail  fail  fail","","branch", proname, argnum,inputclausenum);
				else fprintf(outlsfile, "%-20s%-20s%s/%d.r.end  fail  fail  fail\n", "","branch",proname, argnum);
			}

	/*if(mlvmclausetype==MLVMFACT )
	fprintf(outlsfile, "%-20s%-20s\n","","proceed");*/
}

void goal_set(MLVMTERM* t, int option)       /* do not use cset* here, for confusing */
{
    switch (t->type){
            /*case REF: */
			case VAR:
                      /*if ((t->index != t->annot) && (t->level>1)) fprintf(outlsfile, "csetval %d \n", t->annot);*/ /*[csetval t->annot]; */
                      /*else if(t->index==t->annot) fprintf(outlsfile, "csetvar\n"); *//*[csetvar ]; */
					  if ((t->index != t->annot) && (t->level>1)) fprintf(outlsfile, "%-20s%-20s%d  %d \n","","setval", t->index, t->annot); /*[csetval t->annot]; */
                      else if(t->index==t->annot && ((mlvmclausetype!=MLVMCHAIN && t->level !=1) || (mlvmclausetype==MLVMCHAIN && t->level!=0))) fprintf(outlsfile, "%-20s%-20s%d\n","","setvar", t->index);
                      else if(t->index==t->annot && option==1 ) fprintf(outlsfile, "%-20s%-20s%d\n", "","setvar",t->index); /*[csetvar ]; */
                      return ;
			case VOIDVAR:
                 	  if ((t->index != t->annot) &&
                 	  ((mlvmclausetype!=MLVMCHAIN && t->level >1) || (mlvmclausetype==MLVMCHAIN && t->level>0)))
                 	  fprintf(outlsfile, "%-20s%-20s%d\n","","setvar", t->index);
                      return ;

            /*case CON: */
			case ATOM:
                      /*fprintf(outlsfile, "csetcon '%s'\n", t->node->name);  */ /*[csetcon t->node->name]; */
					  fprintf(outlsfile, "%-20s%-20s%d  '%s'\n", "","setcon",t->index, t->node->name);
                      return ;
			case MLVMNIL:
                      /*fprintf(outlsfile, "csetcon '%s'\n", t->node->name);  */ /*[csetcon t->node->name]; */
					  fprintf(outlsfile, "%-20s%-20s%d  '%s'\n", "","setcon",t->index, t->node->name);
                      return ;
		/**************************need to be further considered ***************/
            /*case FLP: */
			case REAL:
            		  /*fprintf(outlsfile, "csetfloat  %f\n", t->rval);*//*          [csetfloat t->node->name]; */
					  fprintf(outlsfile, "%-20s%-20s%d  %f\n","","setfloat", t->index, t->rval);
                      return ;
			case MLVMINT:
						if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
						{
							if(t->level>=2)
							fprintf(outlsfile, "%-20s%-20s%d  %d\n", "","setint",t->index, (int) t->rval);
                      		return ;
						}
						else if(mlvmclausetype==MLVMCHAIN)
						{
							if(t->level>=1)
							fprintf(outlsfile, "%-20s%-20s%d  %d\n", "","setint",t->index, (int) t->rval);
                      		return ;
						}
            case MLVMLIS:
                      fprintf(outlsfile, "%-20s%-20s%d  %d\n","","setlist", t->index, t->annot); /*[csetlist t->annot]; */
                      return ;
            case MLVMLVS:
			          fprintf(outlsfile, "%-20s%-20s%d  %d\n","","setlist", t->index, t->annot); /*[csetlist t->annot]; */
                      return ;

            case STR:
					if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
					{
						if(t->level>=1) fprintf(outlsfile, "%-20s%-20s%d  %d\n", "","setstr",t->index, t->annot);
					}
					else if(mlvmclausetype==MLVMCHAIN)
					{
						if(t->level>=0) fprintf(outlsfile, "%-20s%-20s%d  %d\n", "","setstr",t->index, t->annot);
					}

             /* according to my analysis, only the nested nested STR need to setstr */
          /*  if(t->level>=2)
					  fprintf(outlsfile, "%-20s%-20s%d  %d\n", "","setstr",t->index, t->annot);*/
                      /*fprintf(outlsfile, "csetstr %d\n", t->annot);*/ /*[csetstr t->annot]; */
                      return ;
        }
}

void goal_put(MLVMTERM* t)
{
	MLVMTERM *sib;
	sib = t->kids;
	while (sib)
	{
    	switch (sib->type){
            /*case REF: */
			case VAR:
                      if(t->type!=MLVMTERMCOMPARISON)
                      {
						  if(istest((int) t->rval))
						  {
							  if(sib->index==sib->annot) fprintf(outlsfile, "%-20s%-20s%d\n","","setvar", sib->index); /*[csetvar ]; */
							  isvarcell=sib->annot;
						  }
						  else
						  {
						  if (sib->index != sib->annot)
                      		{
							    fprintf(outlsfile, "%-20s%-20s%d \n","","putval", sib->annot); /*[csetval sib->annot]; */

					  		}
                      		else if(sib->index==sib->annot)
                      		{
							    fprintf(outlsfile, "%-20s%-20s%d \n","","putvar", sib->annot); /*[csetvar ]; */

					  		}
						}
						}
					  else
					  {
						  if(firsttermeq==-1)
						  	firsttermeq=sib->annot;
						  else if(secondtermeq==-1)
						  		secondtermeq=sib->annot;
					  }

                      break ;
            /*case CON: */
			case ATOM:
                      fprintf(outlsfile, "%-20s%-20s'%s'\n","","putcon", sib->node->name);
                      break ;
			case MLVMATOMCOMP: /* very special constant , can not use putcon */
                      fprintf(outlsfile, "%-20s%-20s%d  '%s'\n","","setcon", sib->index, sib->node->name);
                      if(firsttermeq==-1)
					   	firsttermeq=sib->index;
					  else if(secondtermeq==-1)
						secondtermeq=sib->index;
                      break ;

			case MLVMNIL:
                      fprintf(outlsfile, "%-20s%-20s'%s'\n","","putcon", sib->node->name);   /*[csetcon sib->node->name]; */
                      break ;
		/**************************need to be further considered ***************/
            /*case FLP: */
			case REAL:
            		  /*fprintf(outlsfile, "putfloat or putstr  %d\n", sib->index); *//*         [csetfloat sib->node->name]; */
					   if(t->type!=MLVMTERMCOMPARISON)                                 /* why  sib->index ?????? */
					  fprintf(outlsfile, "%-20s%-20s%d\n","","putstr", sib->annot);
					   else
					   {
					  	  if(firsttermeq==-1)
					  		  	firsttermeq=sib->index;
					      else if(secondtermeq==-1)
					    		secondtermeq=sib->index;
					   }

                      break ;
			case MLVMINT:
			          fprintf(outlsfile, "%-20s%-20s%d\n", "","putint",(int) sib->rval);
                      break ;
            case MLVMLIS:
                      /*fprintf(outlsfile, "putlist  %d \n", sib->index );*/
                      if(t->type!=MLVMTERMCOMPARISON)
                      fprintf(outlsfile, "%-20s%-20s%d \n","","putval", sib->index );
					   else
					   {
					  	  if(firsttermeq==-1)
					  		  	firsttermeq=sib->index;
					      else if(secondtermeq==-1)
					    		secondtermeq=sib->index;
					   }
                      break ;
            case MLVMLVS:
			          /*fprintf(outlsfile, "putlist  %d \n", sib->index );*/
			          if(t->type!=MLVMTERMCOMPARISON)
			          fprintf(outlsfile, "%-20s%-20s%d\n","","putval", sib->index );
					   else
					   {
					  	  if(firsttermeq==-1)
					  		  	firsttermeq=sib->index;
					      else if(secondtermeq==-1)
					    		secondtermeq=sib->index;
					   }
					  break ;
            case STR:
            			if(t->type!=MLVMTERMCOMPARISON)
					  fprintf(outlsfile, "%-20s%-20s%d\n","","putstr", sib->annot); /*Atten: not sib->annot]; */
					   else
					   {
					  	  if(firsttermeq==-1)
					  		  	firsttermeq=sib->index;
					      else if(secondtermeq==-1)
					    		secondtermeq=sib->index;
					   }
					  break ;
            case VOIDVAR:
					  fprintf(outlsfile, "%-20s%-20s\n","","putvoid"); /*for void var "_" */
                      break ;
        }
		sib = sib->next;
	}

}

void intostack1(MLVMTERM *t) /* in fact a preorder traversal */
{
	if(t!=NULL)
	{
		/*fprintf(outlsfile, "t->node->name=%s\n",   t->node->name);*/
		pushmlvmmath1(t);
		intostack1(t->kids);
		if((t->level!=0 && (mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)) ||
		    mlvmclausetype==MLVMCHAIN )             /* only push one math expression one time in multiple goals clause */
			intostack1(t->next);
	}
}

int math_set(MLVMMATHSTACKTERM* t, int cellnum)
{
    switch (t->type){
     		case VAR:
                      return t->address;
            case REAL:
					  fprintf(outlsfile, "%-20s%-20s%d  %f\n", "","setfloat",cellnum, t->floatval);
                      return cellnum;
			case MLVMINT:
			          fprintf(outlsfile, "%-20s%-20s%d  %d\n", "","setint",cellnum,  t->intval);
                      return cellnum;
			case MLVMINTERMEDVAL:
					  return t->address;
        }
}

math_op(MLVMTERM *t, int leftarg, int rightarg, int destcell)
{
	switch ((int) t->rval){
	     		case 20:                     /*  +  */
						fprintf(outlsfile, "%-20s%-20s%d  %d  %d\n", "","add",leftarg, rightarg,destcell );
						pushmlvmmath2(MLVMINTERMEDVAL, 0, 0, destcell);
						return;

	            /*case 21:*/                     /* signed  - */

				case 22:                     /*  -  */
				        fprintf(outlsfile, "%-20s%-20s%d  %d  %d\n", "","sub",leftarg, rightarg, destcell);
						pushmlvmmath2(MLVMINTERMEDVAL, 0, 0, destcell);
						return;
				case 23:                     /*  *  */
						fprintf(outlsfile, "%-20s%-20s%d  %d  %d\n", "","mul",leftarg, rightarg, destcell);
						pushmlvmmath2(MLVMINTERMEDVAL, 0, 0, destcell);
						return;
				case 24:                     /*  /  */
						fprintf(outlsfile, "%-20s%-20s%d  %d  %d\n", "","div",leftarg, rightarg, destcell);
						pushmlvmmath2(MLVMINTERMEDVAL, 0, 0, destcell);
						return;
				case MODVAL:                     /*  mod  */
						fprintf(outlsfile, "%-20s%-20s%d  %d  %d\n", "","mod",leftarg, rightarg, destcell);
						pushmlvmmath2(MLVMINTERMEDVAL, 0, 0, destcell);
						return;
				case ISVAL:                   /* is */
				        /*if(leftarg<mlvmallocn2)*/
						/*fprintf(outlsfile, "%-20s%-20s%d\n", "","setvar",leftarg);*/
						fprintf(outlsfile, "%-20s%-20s%d  %d\n", "","is",rightarg, leftarg);
						return;
				case EQVAL:                   /* == */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","ifeq",leftarg, rightarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","ifeq",leftarg, rightarg);
						return;
				case NEVAL:                   /* /== */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","ifne",leftarg, rightarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","ifne",leftarg, rightarg);
						return;
				case LTVAL:                   /* < */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","iflt",leftarg, rightarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","iflt",leftarg, rightarg);
						return;
			    case GTVAL:                   /* > */
			    		if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","ifgt",leftarg, rightarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","ifgt",leftarg, rightarg);
						return;
				case LEVAL:                   /* <= */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","ifle",leftarg, rightarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","ifle",leftarg, rightarg);
						return;
				case GEVAL:                   /* >= */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","ifge",leftarg, rightarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","ifge",leftarg, rightarg);
						return;
        }

}

void math_code(MLVMTERM *op, MLVMMATHSTACKTERM *left, MLVMMATHSTACKTERM *right)
{
	int arg1, arg2, destcell;
	if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) /* temporary cells are 3 and 4 for intermediate value */
	{                                                                        /*  5 and 6 for arguments passing */
		arg1=math_set(left, 5);
		arg2=math_set(right, 6);
		if(left->type==MLVMINTERMEDVAL) destcell=left->address;
		else if(right->type==MLVMINTERMEDVAL) destcell=right->address;
		else
		{
			if(intermediatevalue==3) intermediatevalue=4;
			else if(intermediatevalue==4) intermediatevalue=3;
			destcell=intermediatevalue;
		}
		math_op(op, arg1, arg2, destcell);
	}
	else                                                  /* temporary cells are 0 and 1 for intermediate value for MLVMFACT and MLVMCHAIN */
	{                                                     /* 2 and 3 for arguments passing */
		arg1=math_set(left, 2);
		arg2=math_set(right, 3);
		if(left->type==MLVMINTERMEDVAL) destcell=left->address;
		else if(right->type==MLVMINTERMEDVAL) destcell=right->address;
		else
		{
			if(intermediatevalue==0) intermediatevalue=1;
			else if(intermediatevalue==1) intermediatevalue=0;
			destcell=intermediatevalue;
		}
		math_op(op, arg1, arg2, destcell);
	}
}

goal_math_clause(FILE *inputfile, MLVMTERM *t)
{
	MLVMMATHSTACKTERM *leftarg, *rightarg;
	intostack1(t);
	while(t=popmlvmmath1())
	{
		if(t->type!=MLVMOPERATOR  && t->type!=MLVMCOMPARISON && t->type!=MLVMIFCOMPARISON )
		{
			if(t->type==VAR)
			{
				if(t->index==t->annot) fprintf(inputfile, "%-20s%-20s%d\n","","setvar", t->index);
				pushmlvmmath2(t->type, 0, 0, t->annot);
			}
			if(t->type==MLVMINT) pushmlvmmath2(t->type, (int) t->rval, 0, -1);
			if(t->type==REAL) pushmlvmmath2(t->type, 0 , t->rval, -1);
		}
		else
		{
			leftarg=popmlvmmath2();
			rightarg=popmlvmmath2();
			math_code(t, leftarg, rightarg);
		}
	}
}

goal_math_chaincall(MLVMTERM *t)
{
	MLVMMATHSTACKTERM *leftarg, *rightarg;
	intostack1(t); /*printmlvmmath1();*/
	while(t=popmlvmmath1())
	{
		if(t->type!=MLVMOPERATOR && t->type!=MLVMCOMPARISON) /* if not operator, just push them into stack2 */
		{
			if(t->type==VAR) pushmlvmmath2(t->type, 0, 0, t->annot);
			if(t->type==MLVMINT) pushmlvmmath2(t->type, (int) t->rval, 0, -1);
			if(t->type==REAL) pushmlvmmath2(t->type, 0 , t->rval, -1);
		}
		else               /* if is operator, do calculation */
		{
			leftarg=popmlvmmath2();
			rightarg=popmlvmmath2();
			math_code(t, leftarg, rightarg);
		}
	}

}

term_comparison(MLVMTERM *t, int infirstarg, int insecondarg)
{
	switch ((int) t->rval){
	     		case UEQVAL:                     /*  =  */
						fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n","","unifiable", infirstarg, insecondarg);
						return;
                case UNEVAL:                     /*  /=  */
						fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n","","nonunifiable", infirstarg, insecondarg);
						return;
	            case TIDENTICAL:                   /* == */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","identical",infirstarg, insecondarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","identical",infirstarg, insecondarg);
						return;
				case TNONIDENTICAL:                   /* /== */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","nonidentical",infirstarg, insecondarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","nonidentical",infirstarg, insecondarg);
						return;
				case TPRECEDE:                   /* == */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","precede",infirstarg, insecondarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","precede",infirstarg, insecondarg);
						return;
				case TPREIDENTICAL:                   /* == */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","preidentical",infirstarg, insecondarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","preidentical",infirstarg, insecondarg);
						return;
				case TFOLLOW:                   /* == */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","follow",infirstarg, insecondarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","follow",infirstarg, insecondarg);
						return;
				case TFLOIDENTICAL:                   /* == */
						if(t->type==MLVMIFCOMPARISON) fprintf(outlsfile, "%-20s%-20s%d  %d  %s/%d.if.%d\n", "","floidentical",infirstarg, insecondarg, curproname, curargnum, ifthenelseno);
						else fprintf(outlsfile, "%-20s%-20s%d  %d  fail\n", "","floidentical",infirstarg, insecondarg);
						return;
						/* = */
        }

}

void goal_stream( char *proname, int argnum, MLVMTERM* t)
{
	MLVMTERM *pregoal;
	int goalnum=0, systemmathexp=0, enterif=0;
    MLVMTERM* sib;
	pregoal=t;
	if(ifcomparison!=NULL && beforeifthenelse==ifcomparison) ifcomparison->type=MLVMIFCOMPARISON;
	if(deeplabel!=-1) fprintf(outlsfile, "%-20s%-20s%d\n","","deeplabel", deeplabel);
    do{
		/*if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE )*/
		if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE )
		{
			if(ownifthenelse==1 && (int) t->rval==SIMECOLONVAL ) enterif=1;
			if(enterif==2 && t->level==0) fprintf(outlsfile, "%s/%d.if.%d:\n", proname, argnum, ifthenelseno++);
			if(enterif==1 && t->level==0) enterif++;
			/*if(t->level==0 )*/
			if((t->level<=0 && t->type == STR) || (t->level==0 && t->type==ATOM) || (t->level==0 && t->type==MLVMNIL)
			   || (t->type==MLVMATOMCOMP)
			   || (t->type==MLVMNECKCUT ) || (t->type==MLVMDEEPCUT)
			   || (t->type==MLVMTERMCOMPARISON)
			  || (t->level<=0 && (t->type==MLVMOPERATOR || t->type==MLVMCOMPARISON || t->type==MLVMIFCOMPARISON)))  /* second condition for the goal like "name/0"*/
			                                          /* the beginning of next goal, at this moment, we need to create "put" series
			                                          for the previous goal */
			{
				goalnum++;
				systemmathexp=0;
				if(goalnum>=2  && pregoal->type!=MLVMOPERATOR && pregoal->type!=MLVMCOMPARISON && pregoal->type!=MLVMIFCOMPARISON && pregoal->type!=SYMCOMMAVAL)  /* enter the next goal */
				{
					if(pregoal->type==MLVMTERMCOMPARISON)
					{
						firsttermeq=-1;
						secondtermeq=-1;
					}
					goal_put(pregoal);
					if(pregoal->type!=MLVMDEEPCUT && pregoal->type!=MLVMNECKCUT && pregoal->type!=MLVMATOMCOMP)
					{
						if(pregoal->rval>250)
						{
							 fprintf(outlsfile, "%-20s%-20s%s/%d\n", "","call",pregoal->node->name, pregoal->argunum);
						}
						else if(pregoal->type!=MLVMTERMCOMPARISON){
								if(isoutput(outlsfile,pregoal->rval)==0)
								{
									if(pregoal->rval!=FAILVAL) fprintf(outlsfile, "%-20s%-20s%s/%d\n", "","builtin",pregoal->node->name, pregoal->argunum); /*systemmathexp=1;*/
									else fprintf(outlsfile, "%-20s%-20s\n","","fail");
								}
							}
						else term_comparison(pregoal, firsttermeq, secondtermeq);
					}
					else
					{
						if(pregoal->type==MLVMNECKCUT) fprintf(outlsfile, "%-20s%-20s\n","", "neckcut");
						if(pregoal->type==MLVMDEEPCUT) fprintf(outlsfile, "%-20s%-20s%d\n", "","deepcut",deeplabel);

					}
				}
				pregoal=t;
			}
		}
    	if (t->type == STR || t->type==MLVMTERMCOMPARISON)
		{
			if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
			{
				if(t->level>=1)
				{
					if(t->index>t->annot && t->annot<0)
					fprintf(outlsfile, "%-20s%-20s%d  %s/%d\n", "","setfun",t->index, t->node->name, t->argunum);
					else fprintf(outlsfile, "%-20s%-20s%d  %s/%d\n", "","setfun",t->annot, t->node->name, t->argunum);
				}
			}
			else if(mlvmclausetype==MLVMCHAIN)
			{
				if(t->level>=0)
				{
					if(t->index>t->annot && t->annot<0)
					fprintf(outlsfile, "%-20s%-20s%d  %s/%d\n", "","setfun",t->index, t->node->name, t->argunum);
					else fprintf(outlsfile, "%-20s%-20s%d  %s/%d\n", "","setfun",t->annot, t->node->name, t->argunum);
				}
			}

			/* only the nested STR need to setfun *//* using "setfun" better than "csetfun" */
			/*if(t->level>=1)
			{
				if(t->index>t->annot && t->annot<0)
				fprintf(outlsfile, "%-20s%-20s%d  %s/%d\n", "","setfun",t->index, t->node->name, t->argunum);
				else fprintf(outlsfile, "%-20s%-20s%d  %s/%d\n", "","setfun",t->annot, t->node->name, t->argunum);
			}*/
			sib = t->kids;
			while (sib)
			{
				if(sib->type==ATOM || sib->type==MLVMNIL || sib->type==MLVMINT)
				{
					if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
					{
						if(sib->level>1) goal_set(sib, 0);
					}
					else if(mlvmclausetype==MLVMCHAIN)
					{
						if(sib->level>0) goal_set(sib, 0);
					}
					/*if(sib->level>1) goal_set(sib, 0);*/
				}
				else if(t->type==MLVMTERMCOMPARISON) goal_set(sib, 1);
				     else goal_set(sib, 0);
				sib = sib->next;
			}
		}
		else if(t->type == MLVMLIS || t->type == MLVMLVS) /* for all LIST are in the STR , */
		/*else if(t->type == MLVMLIS )*/
		{
			/*if(t->level>=1) */      /* only the nested STR need to setfun */
			/*	fprintf(outlsfile, "setlist	%d  \n", t->index);*/
			sib = t->kids;
			while (sib)
			{
				goal_set(sib,0);
				sib = sib->next;
			}
		}
		else if(t->type==MLVMOPERATOR || t->type==MLVMCOMPARISON || t->type==MLVMIFCOMPARISON )
		{
			if(mlvmclausetype==MLVMCHAIN && t->level==-1)
			{
				goal_math_chaincall(t);
				systemmathexp=1;
			}
			if((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && t->level==0)
			{
				goal_math_clause(outlsfile,t);
				systemmathexp=1;
			}
		}
		if (t->next) pushmlvm(t->next);
        if ( t->kids) pushmlvm(t->kids);
	   /*if (t->kids) pushmlvm(t->kids);*/
    } while (t = popmlvm());
    	if(systemmathexp==0)
		{
			if(pregoal->type==MLVMTERMCOMPARISON)
			{
				firsttermeq=-1;
				secondtermeq=-1;
			}
		goal_put(pregoal); /* the last goal */
		if(pregoal->rval>=250)   /* user defined predicates */
		{
			if(mlvmclausetype==MLVMCHAIN ||
			   ((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==1) ||
		       ((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==1 && lastgoal==1))
			   if(pregoal->type!=MLVMNECKCUT )
				{
					 fprintf(outlsfile, "%-20s%-20s%s/%d\n","","chaincall", pregoal->node->name, pregoal->argunum);

				}
				else fprintf(outlsfile, "%-20s%-20s\n","","neckcut");
			/*if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)*/
			if((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && (userdefgoal>1 || (userdefgoal==1 && lastgoal==0)))
				if(pregoal->type!=MLVMDEEPCUT && pregoal->type!=MLVMNECKCUT)
				{


					if(pregoal->rval>250)
					{
						 fprintf(outlsfile, "%-20s%-20s%s/%d\n","","lastcall", pregoal->node->name, pregoal->argunum);

					}
					else if(pregoal->type!=MLVMTERMCOMPARISON)
					      {
							if(isoutput(outlsfile,pregoal->rval)==0)
							 {
								if(pregoal->rval!=FAILVAL) fprintf(outlsfile, "%-20s%-20s%s/%d\n","","builtin", pregoal->node->name, pregoal->argunum);
							 	else fprintf(outlsfile, "%-20s%-20s\n","","fail");
							 }
						  }
					    else term_comparison(pregoal, firsttermeq, secondtermeq);
				}
				else
				{
					if(pregoal->type==MLVMNECKCUT) fprintf(outlsfile, "%-20s%-20s\n","","neckcut");
					if(pregoal->type==MLVMDEEPCUT) fprintf(outlsfile, "%-20s%-20s%d\n","","deepcut", deeplabel);
				}
			   if((mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==0)
			   {
   					if(pregoal->rval!=FAILVAL) {fprintf(outlsfile, "%-20s%-20s%\n","","proceed");}
					else fprintf(outlsfile, "%-20s%-20s\n","","fail");
			   }

		}
		else    /* system predicates or math expression */
		{


			if(mlvmclausetype==MLVMCHAIN )
				if(pregoal->type!=MLVMNECKCUT)
				{
					if(pregoal->type!=MLVMTERMCOMPARISON)
					{
						if(isoutput(outlsfile,pregoal->rval)==0)
						{
							if(pregoal->rval!=FAILVAL) fprintf(outlsfile, "%-20s%-20s%s/%d\n", "","builtin",pregoal->node->name, pregoal->argunum);
							else fprintf(outlsfile, "%-20s%-20s\n","","fail");
						}
					}
                     else term_comparison(pregoal, firsttermeq, secondtermeq);
				}
				else fprintf(outlsfile, "%-20s%-20s\n","","neckcut");

			if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE)
				if(pregoal->type!=MLVMDEEPCUT && pregoal->type!=MLVMNECKCUT && pregoal->type!=MLVMATOMCOMP)
				{
					if(pregoal->rval>250)
					{
						 fprintf(outlsfile, "%-20s%-20s%s/%d\n", "","call",pregoal->node->name, pregoal->argunum);

					}
					else if(pregoal->type!=MLVMTERMCOMPARISON)
					{
						if(isoutput(outlsfile,pregoal->rval)==0)
						{
							if(pregoal->rval!=FAILVAL) fprintf(outlsfile, "%-20s%-20s%s/%d\n","","builtin", pregoal->node->name, pregoal->argunum);
							else fprintf(outlsfile, "%-20s%-20s\n","","fail");
						}
					}
					else term_comparison(pregoal, firsttermeq, secondtermeq);
				}
				else
				{
					if(pregoal->type==MLVMNECKCUT) fprintf(outlsfile, "%-20s%-20s\n","","neckcut");
					if(pregoal->type==MLVMDEEPCUT) fprintf(outlsfile, "%-20s%-20s%d\n","","deepcut", deeplabel);
				}
			if((mlvmclausetype==MLVMCHAIN || mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==0) fprintf(outlsfile, "%-20s%-20s\n","","proceed");
			else fprintf(outlsfile, "%-20s%-20s\n","","return");
		}
	}
	else if((mlvmclausetype==MLVMCHAIN || mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE) && userdefgoal==0) fprintf(outlsfile, "%-20s%-20s\n","","proceed");
			else fprintf(outlsfile, "%-20s%-20s\n","","return");
}


void makeoptimdecision(int inputtype, int inclauseNO, char* inname)
{

	int ii=0, sametype=0;
	if(inputtype==ATOM || inputtype==MLVMNIL)
	{
		hashentrynum++;
		hashentry[inclauseNO]=inname;
	}
	if(inputtype==VAR || inputtype==VOIDVAR) OPTIMDECISION=-2;/* for VAR */
	else
	{
		while(ii<inclauseNO && sametype==0)
		{
			if((optimclausetype[ii]==inputtype) || (optimclausetype[ii]==ATOM && inputtype==MLVMNIL)
			|| (optimclausetype[ii]==MLVMNIL && inputtype==ATOM)) sametype=1;
			else ii++;
		}
		if(sametype==0)
		{
			optimclausetype[inclauseNO]=inputtype;
			if(inputtype==ATOM || inputtype==MLVMNIL)
			{
				sprintf(switch4label[1], "%s/%d.op.1", curproname, curargnum);
				switch4labelpos[inclauseNO]=1;
			}
			else if(inputtype==MLVMLIS || inputtype==MLVMLVS )
			{
				sprintf(switch4label[2], "%s/%d.op.2", curproname, curargnum);
				switch4labelpos[inclauseNO]=2;
			}
			else if(inputtype==STR)
			{
				sprintf(switch4label[3], "%s/%d.op.3", curproname, curargnum);
				switch4labelpos[inclauseNO]=3;
			}
		}
		else
		{
			if((optimclausetype[ii]==inputtype) && (inputtype==ATOM || inputtype==MLVMNIL))
				OPTIMDECISION=-1;
			else OPTIMDECISION=-2;
		}
	}
	if(OPTIMDECISION==0 && (totalclausenum==inclauseNO+1)) /* last clause */
	{
		OPTIMDECISION=1;  printf("final decision is=%d\n", OPTIMDECISION); /* for lastswitch */
		sprintf(switch4label[0], "%s/%d.op.0", curproname, curargnum);
	}
	else if(OPTIMDECISION==-1 && (totalclausenum==inclauseNO+1)) /* last clause */
	{
		OPTIMDECISION=2;  printf("final decision is=%d\n", OPTIMDECISION); /* for lastswitch */
		sprintf(switch4label[0], "%s/%d.op.0", curproname, curargnum);
	}
}

int getparametertype(int parameterNo, MLVMTERM *clausehead, int clauseNO) /* still need to convert original type into MLVM type */
{
  int ii=1;
  MLVMTERM *tempclause;
  tempclause=clausehead->kids;
  while(ii<parameterNo)
  {
	  ii++;
	  tempclause=tempclause->next;
  }
  if(OPTIMDECISION==0 || OPTIMDECISION==-1) makeoptimdecision(tempclause->type, clauseNO, tempclause->node->name);
  return (tempclause->type);

}

void init_param()
{
	int ii;
	for(ii=0; ii<MAX_HASHENTRY_NUM; ii++)
	{
		hashentry[ii]=NULL;
	}
	strcpy(switch4label[0],"fail");
	strcpy(switch4label[1],"fail");
	strcpy(switch4label[2],"fail");
	strcpy(switch4label[3],"fail");
	totalclausenum=0;
	hashentrynum=0;
	switchargno=0;
}

void doAPO(HASH_CLAUSE_NODE_POINTER hhpp)
{
	MLVMTERM *clausehead;
	CLAUSE *tempclause;
	int clausenum=0;
	int doingargno=1, firstassigarg=0;
	while(OPTIMDECISION<=0 && doingargno!=0)
	{
	if(hhpp!=NULL)
	{
		/*printtree(hhpp->first_clause->t, 0, NULL);*/
		clausetype(hhpp->first_clause->t);
		clausehead=mlvmtermtree(hhpp->first_clause->t);
		assign_cell(clausehead, mlvmallocn2+clausehead->argunum);/* needed */
		/*printmlvmtree(clausehead);*/
		curproname=clausehead->node->name;
		curargnum=clausehead->argunum;
		if(firstassigarg==0)
		{
			doingargno=curargnum;
			firstassigarg=1;
		}
		if(OPTIMDECISION<=0 )
			printf("decision=%d, the %d parameter1 is %d type \n", OPTIMDECISION, doingargno, getparametertype(doingargno, clausehead, clausenum));
		tempclause=hhpp->first_clause->next;
		while(tempclause!=NULL)
		{
			clausenum++;
		/*	printtree(tempclause->t, 0, NULL);*/
			clausetype(tempclause->t);
			clausehead=mlvmtermtree(tempclause->t);
			assign_cell(clausehead, mlvmallocn2+clausehead->argunum);/* needed */
			/*printmlvmtree(clausehead);*/
			curproname=clausehead->node->name;
			curargnum=clausehead->argunum;
			if(OPTIMDECISION<=0)
				printf("decision=%d, the %d parameter1 is %d type \n", OPTIMDECISION, doingargno, getparametertype(doingargno, clausehead, clausenum));
			tempclause=tempclause->next;
		}
	}
	if(OPTIMDECISION<=0)
	{
		OPTIMDECISION=0;
		init_param();
		clausenum=0;
		totalclausenum=hhpp->n_clauses;  /* do not do the optimization if clause number greater than 3 */
		if(totalclausenum>3) OPTIMDECISION=-1;
	}
	doingargno--;
	}
	if(OPTIMDECISION>0)
	{
		switchargno=doingargno+1;
	}
}

void mlvmcodegenimagooptim(HASH_CLAUSE_NODE_POINTER hhpp, IMAGONAMENODE *inimagonode, FILE *infilep)
{
	MLVMTERM *clausehead;
	MLVMCLAUSELINK *tempclauselink, *tempclauselink1;
	MLVMCLAUSELINK *clauselinkhead;     /* the head of the switch4label link */
	CLAUSE *tempclause;
	int clausenum=0;
	OPTIMDECISION=0;
	init_param();
	totalclausenum=hhpp->n_clauses;  /* do not do the optimization if clause number greater than 3 */
	if(totalclausenum>3) OPTIMDECISION=-1;
	if(PRINTTAB==1) print_hashfunctab();
	doAPO(hhpp);
	tempclauselink=clauselinkhead;
	clausenum=0;
	if(hhpp!=NULL)
	{
		if(PRINTTAB==1) printtree(hhpp->first_clause->t, 0, NULL);
		clausetype(hhpp->first_clause->t);
		clausehead=mlvmtermtree(hhpp->first_clause->t);
		assign_cell(clausehead, mlvmallocn2+clausehead->argunum);
		if(PRINTTAB==1) printmlvmtree(clausehead);
		curproname=clausehead->node->name;
		curargnum=clausehead->argunum;
		if(clausehead->argunum!=0) read_stream(clausehead->node->name, clausehead->argunum, clausehead->kids, clausenum, hhpp->n_clauses,tempclauselink );
		else read_stream0(clausehead->node->name, clausehead->argunum, clausenum, hhpp->n_clauses, tempclauselink);
		if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE )
			goal_stream(clausehead->node->name, clausehead->argunum,clausehead->next->kids);
		if(mlvmclausetype==MLVMCHAIN )
			goal_stream(clausehead->node->name, clausehead->argunum,clausehead->next);
		if(clausehead->argunum!=0) write_stream(clausehead->node->name, clausehead->argunum, clausehead->kids, clausenum, hhpp->n_clauses);
		tempclause=hhpp->first_clause->next;
		while(tempclause!=NULL)
		{
			fprintf(outlsfile, "\n\n");
			clausenum++;
			if(PRINTTAB==1) printtree(tempclause->t, 0, NULL);
			clausetype(tempclause->t);
			clausehead=mlvmtermtree(tempclause->t);
			assign_cell(clausehead, mlvmallocn2+clausehead->argunum);
			if(PRINTTAB==1) printmlvmtree(clausehead);
			curproname=clausehead->node->name;
			curargnum=clausehead->argunum;
			if(clausehead->argunum!=0) read_stream(clausehead->node->name, clausehead->argunum, clausehead->kids, clausenum, hhpp->n_clauses, tempclauselink);
			else read_stream0(clausehead->node->name, clausehead->argunum, clausenum, hhpp->n_clauses, tempclauselink);
			if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE )
				goal_stream(clausehead->node->name, clausehead->argunum,clausehead->next->kids);
			if(mlvmclausetype==MLVMCHAIN )
				goal_stream(clausehead->node->name, clausehead->argunum,clausehead->next);
			if(clausehead->argunum!=0) write_stream(clausehead->node->name, clausehead->argunum, clausehead->kids, clausenum, hhpp->n_clauses);
			tempclause=tempclause->next;
		}
		/*print_hashfunctab();*/
	}
}

void mlvmcodegenimago(HASH_CLAUSE_NODE_POINTER hhpp, IMAGONAMENODE *inimagonode, FILE *infilep)
{
	MLVMTERM *clausehead;
	CLAUSE *tempclause;
	int clausenum=0;
	if(PRINTTAB==1) print_hashfunctab();
	if(hhpp!=NULL)
	{
		if(PRINTTAB==1) printtree(hhpp->first_clause->t, 0, NULL);
		clausetype(hhpp->first_clause->t);
		clausehead=mlvmtermtree(hhpp->first_clause->t);
		assign_cell(clausehead, mlvmallocn2+clausehead->argunum);
		if(PRINTTAB==1) printmlvmtree(clausehead);
		curproname=clausehead->node->name; /* used by other procedure */
		curargnum=clausehead->argunum;        /* used by other procedures */
		/* for name/0 */
		if(clausehead->argunum!=0) read_stream(clausehead->node->name, clausehead->argunum, clausehead->kids, clausenum, hhpp->n_clauses, NULL);
		else read_stream0(clausehead->node->name, clausehead->argunum, clausenum, hhpp->n_clauses, NULL);
		if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE )
			goal_stream(clausehead->node->name, clausehead->argunum,clausehead->next->kids);
		if(mlvmclausetype==MLVMCHAIN )
			goal_stream(clausehead->node->name, clausehead->argunum,clausehead->next);
		/* only when name/arity, arity!=0 , do following */
		if(clausehead->argunum!=0) write_stream(clausehead->node->name, clausehead->argunum, clausehead->kids, clausenum, hhpp->n_clauses);
		tempclause=hhpp->first_clause->next;
		while(tempclause!=NULL)
		{
			fprintf(outlsfile, "\n\n");
			clausenum++;
			if(PRINTTAB==1)	printtree(tempclause->t, 0, NULL);
			clausetype(tempclause->t);
			clausehead=mlvmtermtree(tempclause->t);
			assign_cell(clausehead, mlvmallocn2+clausehead->argunum);
			if(PRINTTAB==1) printmlvmtree(clausehead);
			curproname=clausehead->node->name;
			curargnum=clausehead->argunum;
			/* for the name/0 clause */
			if(clausehead->argunum!=0) read_stream(clausehead->node->name, clausehead->argunum, clausehead->kids, clausenum, hhpp->n_clauses, NULL);
			else read_stream0(clausehead->node->name, clausehead->argunum, clausenum, hhpp->n_clauses, NULL);
			if(mlvmclausetype==MLVMSPENDCLAUSE || mlvmclausetype==MLVMUSERENDCLAUSE )
				goal_stream(clausehead->node->name, clausehead->argunum,clausehead->next->kids);
			if(mlvmclausetype==MLVMCHAIN )
				goal_stream(clausehead->node->name, clausehead->argunum,clausehead->next);
			if(clausehead->argunum!=0) write_stream(clausehead->node->name, clausehead->argunum, clausehead->kids, clausenum, hhpp->n_clauses);
			tempclause=tempclause->next;
		}
		/*print_hashfunctab();*/
	}
}



HASH_CLAUSE_NODE_POINTER findhashpnode(name, hashtablesize, quick_hash, argunum)
char *name;
int hashtablesize, argunum, quick_hash;
{
	IMAGOTERM *p, *p1;
	int val, found = FALSE;
	int h;
	struct HASH_TAB_NODE *cur;
	if ((cur = hash_name_tab[h=hash(name, hashtablesize)]) != NULL)
			for (; cur != NULL; cur = cur->next)
			{
				if (cur->quick_hash == quick_hash && !strcmp(cur->s, name))
				{
					for (p1 = cur->t; p1; p1 = p1->next)
					if (p1->argunum == argunum)
					{
						return hash_imago_table[p1->val+1];
					}

				}
			}
	return (HASH_CLAUSE_NODE_POINTER) NULL;
}

/* need to optimize it, that only include VAR, constant, list, and str is unnecessary */
void findvarfromstr(char *inputstr)
{
	int ii=0, jj=0,kk=0, nestlevel=0, tempopenquote=0;
	char tempvarname[MAX_NAME_LEN];
	while (chtype[inputstr[ii]] == BS) ii++;
	while(inputstr[ii]!='\0')
 	{
		if(inputstr[ii]=='"')
		{
			if(tempopenquote==0) tempopenquote=1;
			else tempopenquote=0;
		}
	/*if(nestlevel>=0 && !(nestlevel==1 && inputstr[ii]==',') && inputstr[ii]!=' ' && !(nestlevel==1 && inputstr[ii]==')')
	    && !(nestlevel==1 && inputstr[ii]=='!') && (chtype[inputstr[ii]]==N ||
	    chtype[inputstr[ii]]==UC || chtype[inputstr[ii]]==UL
	    || chtype[inputstr[ii]]==LC))*/
	if(nestlevel>=0 && (chtype[inputstr[ii]]==N ||
		    chtype[inputstr[ii]]==UC || chtype[inputstr[ii]]==UL
	    || chtype[inputstr[ii]]==LC)
	    && chtype[inputstr[ii]] != BS && tempopenquote==0)
	{
		tempvarname[jj++]=inputstr[ii];
		if(inputstr[ii]=='[' && tempopenquote==0)
		{
			ii++;
			while(inputstr[ii]!=']' && tempopenquote==0)  /* for the LIS */
			{
				tempvarname[jj++]=inputstr[ii++];
			}
			tempvarname[jj++]=inputstr[ii++];
		}
		if(inputstr[ii]=='"')
		{
			ii++;
			while(inputstr[ii]!='"')  /* string constant within double quote */
			{
				tempvarname[jj++]=inputstr[ii++];
			}
			tempvarname[jj++]=inputstr[ii++];
			/*tempvarname[jj++]=inputstr[ii];*/
		}

	}
	/*if(nestlevel<=1 && (inputstr[ii]==',' || inputstr[ii]==')'))*/
	if((nestlevel==1 || (nestlevel==0 && chtype[inputstr[ii]]!=BK) || chtype[inputstr[ii]]==BS ) && jj!=0
	    && chtype[inputstr[ii]]!=N && chtype[inputstr[ii]]!=UC && chtype[inputstr[ii]]!=UL && chtype[inputstr[ii]]!=LC
	    && tempopenquote==0)

	{

		tempvarname[jj++]='\0';
		if(chtype[tempvarname[0]]!=UL && chtype[tempvarname[0]]!=LC && chtype[tempvarname[0]]!=N) /* only consider VAR */
		{
			kk=0;
			while(kk<varnamenum && strcmp(varname[kk], tempvarname))
			{
				kk++; /* avoid the same symbolic var */
			}
			if(kk==varnamenum) strcpy(varname[varnamenum++], tempvarname);
	    }
		jj=0;
	}
	if(inputstr[ii]==')' && nestlevel!=0 && tempopenquote==0 ) nestlevel--;
	if(inputstr[ii]=='(' && tempopenquote==0 &&
	   ((inputstr[ii-1]>=48 && inputstr[ii-1]<=57) ||     /* integer */
	    (inputstr[ii-1]>=65 && inputstr[ii-1]<=90) ||     /* upper case */
	    (inputstr[ii-1]>=97 && inputstr[ii-1]<=122) )) /* low case */
	    { jj=0; nestlevel++;}
	ii++;
	/*while (chtype[inputstr[ii]] == BS) ii++;*/ /* get rid of control letter */
	}

}

int deepestifthen1(char *inclause1, int *multiif2)
{
	int ii=0, jj=0, nestlevel=0, position=0, greatestnestlevel=-1, clauseheadsym=1, startcountvar=1, tempopenquote=0;
	char tempheadstr[LINE_BUF];
	while(inclause1[ii]!='\0')
	{
		if(inclause1[ii]=='"')
		{
			if(tempopenquote==0) tempopenquote=1;
			else tempopenquote=0;
		}
		if(inclause1[ii]==')' && tempopenquote==0) nestlevel--;
		if(inclause1[ii]=='(' && tempopenquote==0)
		{
			nestlevel++;
			if(clauseheadsym==1)
			{
				clauseheadsym=0;
			}
		}

		if((inclause1[ii]==':' && inclause1[ii+1]=='-') && clauseheadsym==1 && tempopenquote==0)
		{
			clauseheadsym=0;
			headname[ii]='\0';
		}
		if(clauseheadsym==1) headname[ii]=inclause1[ii];
		if(inclause1[ii]=='-' && inclause1[ii+1]=='>' && tempopenquote==0)
		{
			*multiif2=*multiif2+1;  /* this value should greater than 1 */
			ifthenelsenum++;
			if(greatestnestlevel==-1) {greatestnestlevel=nestlevel; position=ii; }
			if((nestlevel==0 && greatestnestlevel==0) || greatestnestlevel>nestlevel)
			{
				greatestnestlevel=nestlevel;
				position=ii;
			}
		}
		ii++;
	}

	return position;
}

code_ifthen(char *inclause1, FILE *outf)
{
	int inclause1oq=0, tempoq1=0;
	char tempstr2[LINE_BUF], tempstr1[LINE_BUF], tempstr3[LINE_BUF], tempstr30[LINE_BUF], *argustr1,*argustr2,*argustr3,createhead1[LINE_BUF],createhead2[LINE_BUF],
	      createbody1[LINE_BUF], createbody2[LINE_BUF], temps[LINE_BUF];
	char tempc;
	int tempcurpos1=0, curpos1=0, curpostemp=0, strpos1=0, strpos2=0, strpos3=0, parenlevel1=0, parenleveltemp=0, parenlevel2=0,
	    ii=0,jj=0,kk=0, parennum1=0, encounterhead=0;

	tempmultiif1=0;
	strcpy(createhead1, "");
	strcpy(createhead2, "");
    curpos1=deepestifthen1(inclause1, &tempmultiif1);
	jj=0;
    kk=0;
	if(curpos1==0)
	{
		fprintf(outf, "%s\n", inclause1);
	}
	else
	{
	strpos1=curpos1;
	strncpy(tempstr1, inclause1, curpos1); /* immediately before -> */
	parenlevel1=0;
	tempstr1[strpos1++]=',';  /* tempstr1 store the ! part */
	tempstr1[strpos1++]='!';
	tempstr1[strpos1++]=',';
	curpos1++; /* read '>' */
	strncpy(tempstr2, inclause1, curpos1-1);
	strpos2=curpos1-1;
	parenleveltemp=0;
	strpos2=curpos1-1;
	tempoq1=0;
	while(strpos2!=0 && !(tempstr2[strpos2]=='-' && tempstr2[strpos2-1]==':') && tempoq1==0) /* just calculate the ( number */
	{
		if(tempstr2[strpos2]=='"')
		{
			if(tempoq1==0) tempoq1=1;
			else tempoq1=0;
		}
		if(tempstr2[strpos2]==')' && tempoq1==0) parenleveltemp--;
		if(tempstr2[strpos2]=='(' && tempoq1==0) parenleveltemp++;
		strpos2--;
	}
	tempoq1=0;
	parenlevel1=0;
	strpos2=curpos1-1;
	while(strpos2!=0 && !(tempstr2[strpos2]==',' && parenlevel1>=0) && encounterhead==0  && tempoq1==0)  /* find the comma backward*/
	{
		if(tempstr2[strpos2]=='"')
		{
			if(tempoq1==0) tempoq1=1;
			else tempoq1=0;
		}
		if(tempstr2[strpos2]==')' && tempoq1==0) parenlevel1--;
		if(tempstr2[strpos2]=='(' && tempoq1==0) parenlevel1++;
		if(tempstr2[strpos2]=='-' && tempstr2[strpos2-1]==':' && tempoq1==0) encounterhead=1;
		strpos2--;

	}
	/*if(strpos2==0)*/
	tempoq1=0;
	if(encounterhead==1) /* can not find comma before encounter :- */
	{
		parenlevel1=0;
		strpos2=curpos1-1;
		while(strpos2!=0 && !(tempstr2[strpos2]=='-' && tempstr2[strpos2-1]==':') && tempoq1==0)
		{
			if(tempstr2[strpos2]=='"')
			{
				if(tempoq1==0) tempoq1=1;
				else tempoq1=0;
			}
			if(tempstr2[strpos2]==')' && tempoq1==0) parenlevel1--;
			if(tempstr2[strpos2]=='(' && tempoq1==0) parenlevel1++;
			strpos2--;
		}
	}
	strpos2++;
	for(ii=0; ii<curpos1-strpos2; ii++)  /* just consider the variable inside the "->" body */
	{
		tempstr30[ii]=inclause1[strpos2+ii];
	}
	tempstr30[ii]='\0';
	for(ii=0; ii<(parenleveltemp-parenlevel1); ii++)
	{
		createbody1[jj++]='(';
	}
	for(ii=strpos2; ii<strpos1; ii++) createbody1[jj++]=tempstr1[ii];
	strncpy(tempstr3, tempstr2, strpos2); /* the preceding context without -> */
	strpos3=strpos2-1;
	/*strcat(tempstr3, ".");*/
	if(encounterhead!=1)
	tempstr3[strpos3]=',';

	strpos3++;
	tempstr3[strpos3]='\0';
	for(ii=0; ii<parenlevel1; ii++)
	{
		tempstr2[strpos2++]='(';
		createbody2[kk++]='(';
		strcat(tempstr3, "(");
	}
	for(ii=0; ii<(parenleveltemp-parenlevel1); ii++)
	{
		createbody2[kk++]='(';
	}

	/********this part just try to position the end of -> **/
	inclause1oq=0;
	tempcurpos1=curpos1;
	parenlevel2=0;
	while( !(inclause1[tempcurpos1+1]==';' && parenlevel2==0 && inclause1oq==0))
	{

		tempcurpos1++;
		if(inclause1[tempcurpos1]=='"')
		{
			if(inclause1oq==0) inclause1oq=1;
			else inclause1oq=0;
		}
		if(inclause1[tempcurpos1]==')' && inclause1oq==0) parenlevel2--;
		if(inclause1[tempcurpos1]=='(' && inclause1oq==0) parenlevel2++;
		temps[0]=inclause1[tempcurpos1];
		temps[1]='\0';
		strcat(tempstr30, temps);

	}
	tempcurpos1++; /* read the ';' */
	tempcurpos1++;
	curpostemp=tempcurpos1;
	parennum1=0;  /* two cases: one contain no parenthesis, end by '.' , another contain '(' and ')' */
	inclause1oq=0;
	while( inclause1[tempcurpos1]!='\0'
			&& !(inclause1[tempcurpos1]=='.' && inclause1oq==0)
			&& !(parennum1>0 && inclause1[tempcurpos1]==',' && inclause1oq==0)
			&& !(parennum1>0 && inclause1[tempcurpos1]==';' && inclause1oq==0) )
	{
		if(inclause1[tempcurpos1]=='"')
		{
			if(inclause1oq==0) inclause1oq=1;
			else inclause1oq=0;
		}
		if(inclause1[tempcurpos1]==')' && inclause1oq==0) parennum1++;
		if(inclause1[tempcurpos1]=='(' && inclause1oq==0) parennum1--;
		temps[0]=inclause1[tempcurpos1];
		temps[1]='\0';
		strcat(tempstr30, temps);
		tempcurpos1++;

	}
	/********this part just try to position the end of -> **/

	sprintf(temps, "___%d", ifthenelsenum);
	strcpy(tempoutstr, tempstr30);
	strcat(tempstr3, headname);
	strcat(createhead1, headname);
	strcat(tempstr3, temps); strcat(createhead1, temps);
	strcat(tempstr3, "("); strcat(createhead1, "(");
	findvarfromstr(tempoutstr);
	for(ii=0; ii<varnamenum; ii++)
	{
		strcat(tempstr3, varname[ii]);strcat(createhead1, varname[ii]);
		if(ii!=varnamenum-1)
		{
			strcat(tempstr3, ",");
			strcat(createhead1, ",");
		}
	}
	strcat(tempstr3, ")"); /* need to consider end by dot or comma */
	strcat(createhead1,"):-");
	parenlevel2=0;
	inclause1oq=0;
	while( !(inclause1[curpos1+1]==';' && parenlevel2==0 && inclause1oq==0))
	{
		curpos1++;
		if(inclause1[curpos1+1]=='"')
		{
			if(inclause1oq==0) inclause1oq=1;
			else inclause1oq=0;
		}
		if(inclause1[curpos1]==')' && inclause1oq==0) parenlevel2--;
		if(inclause1[curpos1]=='(' && inclause1oq==0) parenlevel2++;
		tempstr1[strpos1++]=inclause1[curpos1];
		createbody1[jj++]=inclause1[curpos1];
		/*if((inclause1[curpos1-1]=='-') && inclause1[curpos1]=='>') tempmultiif1++;*/
	}
	strcpy(createhead2, createhead1);
	curpos1++; /* read the ';' */
	curpos1++;
	curpostemp=curpos1;
	parennum1=0;  /* two cases: one contain no parenthesis, end by '.' , another contain '(' and ')' */
	inclause1oq=0;
	while( inclause1[curpos1]!='\0'
			&& !(inclause1[curpos1]=='.' && inclause1oq==0 )
			&& !(parennum1>0 && inclause1[curpos1]==',' && inclause1oq==0)
			&& !(parennum1>0 && inclause1[curpos1]==';' && inclause1oq==0) )
	{
		if(inclause1[curpos1]=='"')
		{
			if(inclause1oq==0) inclause1oq=1;
			else inclause1oq=0;
		}
		/*if((inclause1[curpos1]=='-') && inclause1[curpos1+1]=='>') tempmultiif1++;*/
		if(inclause1[curpos1]==')' && inclause1oq==0) parennum1++;
		if(inclause1[curpos1]=='(' && inclause1oq==0) parennum1--;
		tempstr2[strpos2++]=inclause1[curpos1];  /* the part after ; */
		createbody2[kk++]=inclause1[curpos1];
		curpos1++;
	}
	parenleveltemp=0;
	inclause1oq=0;
	while( (tempc=inclause1[curpostemp])!='\0' )
	{
		if(inclause1[curpostemp]=='"')
		{
			if(inclause1oq==0) inclause1oq=1;
			else inclause1oq=0;
		}
		if(inclause1[curpostemp]==')' && inclause1oq==0) parenleveltemp++;
		if(inclause1[curpostemp]=='(' && inclause1oq==0) parenleveltemp--;
		curpostemp++;
	}
	for(ii=0; ii<parennum1; ii++)
	{
		tempstr1[strpos1++]=')';
		createbody1[jj++]=')';
		strcat(tempstr3, ")");
	}
	for(ii=0; ii<(parenleveltemp-parennum1); ii++)
	{
		createbody1[jj++]=')';
		createbody2[kk++]=')';
	}
	for(; inclause1[curpos1]!='\0'; curpos1++)
	{
		tempstr1[strpos1++]=inclause1[curpos1];
		tempstr2[strpos2++]=inclause1[curpos1];
		temps[0]=inclause1[curpos1];
		temps[1]='\0';
		strcat(tempstr3, temps);
		/*if((inclause1[curpos1]=='-') && inclause1[curpos1+1]=='>') tempmultiif1++;*/
	}

	/*if(temps[0]!='.') strcat(tempstr3, ".");*/
	createbody1[jj++]='.';
	createbody1[jj++]='\0';
	strcat(createhead1, createbody1);
	createbody2[kk++]='.';
	createbody2[kk++]='\0';
	strcat(createhead2, createbody2);
	tempstr1[strpos1++]='\0';
	tempstr2[strpos2++]='\0';
	if(tempmultiif1<=1) {fprintf(outf, "%s\n", tempstr3);fprintf(outf, "%s\n", createhead1);fprintf(outf, "%s\n", createhead2); }
	else
	{
		argustr1=(char *) malloc(LINE_BUF*sizeof(char));
		argustr2=(char *) malloc(LINE_BUF*sizeof(char));
		argustr3=(char *) malloc(LINE_BUF*sizeof(char));
		strcpy(argustr1, createhead1);
		strcpy(argustr2, createhead2);
		strcpy(argustr3, tempstr3);
		code_ifthen(argustr3, outf);
		code_ifthen(argustr1, outf);
		code_ifthen(argustr2, outf);

		free(argustr1);
		free(argustr2);
		free(argustr3);
	}
	}
}


printtemp(FILE *outfile)
{
	int ii;
	if(multiif==0)
	{
		for(ii=0; ii<curtempusermodule; ii++)
			fputc(tempusermodule[ii], outfile);
		for(ii=0; ii<curpos10[curstr]; ii++)
			fputc(tempstr10[curstr][ii], outfile);
	}
	if(multiif==1)
	{
		for(ii=0; ii<curtempusermodule; ii++) multiarrow1[ii]=tempusermodule[ii];
		multiarrow1[ii]='\0';
		varnamenum=0; /* init the varnamenum */
		for(ii=0; ii<50; ii++) /* init the varname for create ifthenelse expression */
		{
			strcpy(varname[ii], "");
		}
		code_ifthen(multiarrow1,outfile);
	}
	strcpy(tempstr10[curstr], "");
	strcpy(tempusermodule, "");
	strcpy(multiarrow1, "");
	curpos10[curstr]=curtempusermodule=multiif=ifthenelsetail=0;
	colonposition=-1;
}


char mlvmpeekchar(FILE *infile) /*** just look, not touch ***/
{
register char c1, c2;
	c1 = ch1;
	c2 = mlvmreadchar(infile, 1);
	mlvmunreadchar(c2);
	ch1 = c1;
	return c2;
}

char mlvmreadchar(FILE *infile, int comment) /*** returns '\0' on EOF ***/
{

	register int c;
	int tempc, ii;
	int parenlevel=0;  /* for the -> backward search */
	if (unread_cnt1 > 0) c = unread_buf1[--unread_cnt1];
	else if ((c = getc(infile)) == EOF)  c = '\0';
	if (c == '\n') {
		lineno1++;
		old_len1 = line_pos1;
		line_pos1 = 0;
	} else
	if (line_pos1 < LINE_BUF) line_buf1[line_pos1++] = c;
	else {
		old_len1 = line_pos1;
		line_pos1 = 0;
		line_buf1[line_pos1++] = c;
	}
	ch1 = c;
	if(comment==0)
	{
		if((c=='/' && mlvmpeekchar(infile)=='*') || (c=='/' && mlvmpeekchar(infile)=='/'))
		;
		else if(c!='\0')
			{
				if(c=='"')
				{
					if(opendoublequote==0) opendoublequote=1;
					else opendoublequote=0;
				}
				if((c=='-') && mlvmpeekchar(infile)=='>' && ifthenelsetail==0 && opendoublequote==0)
				{

					ifthenelsetail=0;
					tempusermodule[curtempusermodule++]=c;
					do{
						tempc=mlvmreadchar(infile, 0);
						if (tempc == '/' && mlvmpeekchar(infile) == '*' && opendoublequote==0) { /* enter the comment */
							mlvmreadchar(infile, 1);       /* read the * symbol */
							while (!((tempc = mlvmreadchar(infile, 1)) == '*' && mlvmpeekchar(infile) == '/' && opendoublequote==0)
        							&& chtype[tempc] != ENDF) ;
									mlvmreadchar(infile, 1); /* end of comment */
						}
						else if( tempc=='/' && mlvmpeekchar(infile)=='/' && opendoublequote==0)
						{
							mlvmreadchar(infile, 1);
							while( (tempc=mlvmreadchar(infile, 1))!='\n') ;
						}
					}while( !(mlvmpeekchar(infile)=='.'  && opendoublequote==0) && tempc!='\0' );
					tempc=mlvmreadchar(infile,0);
					tempusermodule[curtempusermodule]='\0';
					multiif=1;
					needtoprint=1;
					ifthenelsetail=1;
				}
				else
				{
					tempusermodule[curtempusermodule++]=c; /*fputc(c, outfile);*/
				}

				if(c==':' && opendoublequote==0) colonposition=curtempusermodule-1;       /* the position of ":"  */
				if(c=='.' && opendoublequote==0) needtoprint=1;
			}
	}
	return c;
}

mlvmunreadchar(c) /*** NULL for EOF ***/
register char c;
{
	if (c != '\0') {
		if (unread_cnt1 >= LINE_BUF) fatalerror("too many pushbacks");
		unread_buf1[unread_cnt1++]=c;
		if (c == '\n') lineno1--;
		if (line_pos1) line_pos1--;
	}
}



mlvmget_name(char *s,FILE *infile, int rorw) /*** stores a NAME in s, NAME is either variable or symbolic ***/
{
register char c;
	c = ch1;
	if (c == '/' && mlvmpeekchar(infile) == '*') { /* enter the comment */
		mlvmreadchar(infile, 1);       /* read the * symbol */
		while (!((c = mlvmreadchar(infile, 1)) == '*' && mlvmpeekchar(infile) == '/')
	    && chtype[c] != ENDF) ;
		mlvmreadchar(infile, 1); /* end of comment */
	}
	do {
		*s++ = c;
		c = mlvmreadchar(infile, rorw);
	} while (chtype[c] & mask_class_bits(UC | UL | LC | N));
	*s = '\0';
}

printimagonode()
{
	IMAGONAMENODE *temp1;
	temp1=imagonamehead;
	while(temp1!=NULL) { temp1=temp1->next; }
}

addlinkednode(IMAGONAMENODE **inputhead, char *ins)
{
	IMAGONAMENODE *temp;
	temp=(IMAGONAMENODE *) emalloc(sizeof(IMAGONAMENODE));
	temp->filename=(char *) addstrtable(ins);
	temp->next=*inputhead;
	temp->imagotype=imagotype;
	*inputhead=temp;
	/*printimagonode();*/
}

appendusermodule(FILE *outfile, FILE *inusermodulefile)
{
	register char c;
	fseek(inusermodulefile , 0, SEEK_SET); /* the beginning of usermodule file */
	while((c = fgetc(inusermodulefile)) != EOF)  /* end of file */
		fputc(c, outfile);
}

preprocessorimagoscan(FILE *infile, FILE *inusermodulefile)
{
register char c;
char *s, tempfilename[LINE_BUF];
double tens, intpart, fracpart, num;
int sign = 1, esign, expart;
int level;
int hash_sum;
int outsideimago=1;
int getimagoname=0;
FILE *outfile;

c = mlvmreadchar(infile, outsideimago);
while(c != '\0')  /* end of file */
{
	while (chtype[c] == BS)
	{
		c = mlvmreadchar(infile, outsideimago);

	}
	if(outsideimago==0 && getimagoname==0 && mask_class_no(chtype[c])==mask_class_no(LC))
	{

		mlvmget_name(buffer1, infile, outsideimago);
		strcpy(tempfilename, inputfiledir);
		strcat(tempfilename, buffer1);
		strcpy(buffer1, tempfilename);
		getimagoname=1;
		addlinkednode(&imagonamehead, buffer1);
		/*strcpy(tempfile, "./temp/");*/
		/*strcpy(tempfile, "./");*/
		strcpy(tempfile, "");
		strcat(tempfile, tempfilename);
		strcat(tempfile, ".ima");
		/*strcpy(tempfile, buffer1);*/
		outfile=my_open(tempfile, "w");
		curtempusermodule=colonposition;
		printtemp(outfile);
		while((c = mlvmreadchar(infile, 1)) != '.') ;   /* read until '.'  */
	}
	switch(mask_class_no(chtype[c])) {
	case mask_class_no(SY):
		s = buffer1;
		if (ch1 == '.' &&
				(chtype[mlvmpeekchar(infile)] == BS || chtype[mlvmpeekchar(infile)] == ENDF)) {
			mlvmreadchar(infile, outsideimago);
			break;
		} else if (c == '/' && mlvmpeekchar(infile) == '*') { /* enter the comment */
				mlvmreadchar(infile, 1);       /* read the * symbol */
					while (!((c = mlvmreadchar(infile, 1)) == '*' && mlvmpeekchar(infile) == '/')
					         && chtype[c] != ENDF) ;
					mlvmreadchar(infile, 1); /* end of comment */
		}
		else if (c == ':' && mlvmpeekchar(infile) == '-') { /* maybe enter the IMAGO */
				if(outsideimago==1) mlvmreadchar(infile, 1);       /* read the - symbol */
				else mlvmreadchar(infile, 0);
				c=mlvmreadchar(infile, 0);       /* read the first letter of this word */
				if (c == '/' && mlvmpeekchar(infile) == '*') { /* enter the comment */
					mlvmreadchar(infile, 1);       /* read the * symbol */
					while (!((c = mlvmreadchar(infile, 1)) == '*' && mlvmpeekchar(infile) == '/')
				    && chtype[c] != ENDF) ;
					mlvmreadchar(infile, 1); /* end of comment */
				}
				while (chtype[c] == BS)                 /* get rid of space */
					{
						c = mlvmreadchar(infile, 0);
					}
				if (c == '/' && mlvmpeekchar(infile) == '*') { /* enter the comment */
					mlvmreadchar(infile, 1);       /* read the * symbol */
					while (!((c = mlvmreadchar(infile, 1)) == '*' && mlvmpeekchar(infile) == '/')
				    && chtype[c] != ENDF) ;
					mlvmreadchar(infile, 1); /* end of comment */
				}
				mlvmget_name(buffer1, infile, outsideimago);
				if(!(strcmp(buffer1, "stationary")) || !(strcmp(buffer1, "worker"))|| !(strcmp(buffer1, "messenger")))
				{
					outsideimago=0;
					getimagoname=0;
					if(!(strcmp(buffer1, "stationary"))) imagotype=1;
					if(!(strcmp(buffer1, "worker"))) imagotype=2;
					if(!(strcmp(buffer1, "messenger"))) imagotype=3;
				}
				else if(!(strcmp(buffer1, "end_stationary")) || !(strcmp(buffer1, "end_worker"))|| !(strcmp(buffer1, "end_messenger")))
				{
					outsideimago=1;
					curtempusermodule=colonposition;
					printtemp(outfile);
					while((c = mlvmreadchar(infile, 1)) != '.') ;   /* read until the end of IMAGO */
					printtemp(outfile);
					appendusermodule(outfile, inusermodulefile);
					fclose(outfile);
				}
				else if(outsideimago==1) curtempusermodule=0;  /* do not print any thing if is not IMAGO */
		}
		c = ch1;
		do {
			if (c == '/') {
				if (mlvmpeekchar(infile) == '*') {
					break;
				}
			}
			if (c == '/') {
				if (mlvmpeekchar(infile) == '/') {
					break;
				}
			}
			*s++ =c;
		} while (chtype[(c=mlvmreadchar(infile, outsideimago))] == SY && !(c!='.') && !(c!=':'));
		*s = '\0';
		break;
	default:
		c = mlvmreadchar(infile, outsideimago);
		if (c == '/' && mlvmpeekchar(infile) == '*') { /* enter the comment */
			mlvmreadchar(infile, 1);       /* read the * symbol */
			while (!((c = mlvmreadchar(infile, 1)) == '*' && mlvmpeekchar(infile) == '/')
		    && chtype[c] != ENDF) ;
			mlvmreadchar(infile, 1); /* end of comment */
		}
		break;
	}
}  /* the first while */
	printtemp(outfile);

}

createusermodule(FILE *infile, FILE *outfile)
{
	register char c;
	char *s;
	int enterimago=0, curpos=0, curstr=0;
	char tempstr[LINE_BUF],tempstr10[10][LINE_BUF]; /* will change to dynamic allocation later */
	while((c = mlvmreadchar(infile, enterimago)) != '\0')  /* end of file */
	{
		if(needtoprint==1) { printtemp(outfile); needtoprint=0;}
		while (chtype[c] == BS)
		{
			c = mlvmreadchar(infile,enterimago);

		}
		switch(mask_class_no(chtype[c])) {
		case mask_class_no(SY):
			s = buffer1;
			if (ch1 == '.' &&
					(chtype[mlvmpeekchar(infile)] == BS || chtype[mlvmpeekchar(infile)] == ENDF)) {
				mlvmreadchar(infile, enterimago);
				break;
			} else if (c == '/' && mlvmpeekchar(infile) == '*') { /* enter the comment */
					mlvmreadchar(infile, 1);       /* read the * symbol */
						while (!((c = mlvmreadchar(infile, 1)) == '*' && mlvmpeekchar(infile) == '/')
						         && chtype[c] != ENDF) ;
						mlvmreadchar(infile, 1); /* end of comment */
			}
			else if( c=='/' && mlvmpeekchar(infile)=='/')
			{
				mlvmreadchar(infile, 1);
				while( (c=mlvmreadchar(infile, 1))!='\n') ;
			}
			else if (c == ':' && mlvmpeekchar(infile) == '-') { /* maybe enter the IMAGO */
					/*mlvmreadchar(infile, 0); */      /* read the - symbol */
					/*c=mlvmreadchar(infile, 0);*/       /* read the first letter of this word */

					mlvmreadchar(infile, enterimago);       /* read the - symbol */
					c=mlvmreadchar(infile, enterimago);       /* read the first letter of this word */
					while (chtype[c] == BS)                 /* get rid of space */
						{
							c = mlvmreadchar(infile, enterimago);

						}
					mlvmget_name(buffer1, infile, enterimago);
					if(!(strcmp(buffer1, "stationary")) || !(strcmp(buffer1, "worker")) ||!(strcmp(buffer1, "messenger")))
					{
						curtempusermodule=colonposition;
						printtemp(outfile);
						enterimago=1;
					}
					if(!(strcmp(buffer1, "end_stationary")) || !(strcmp(buffer1, "end_worker")) ||!(strcmp(buffer1, "end_messenger")))
						{
							curtempusermodule=colonposition;
							printtemp(outfile);
							enterimago=0;
							while(c!='.')
							{
								c=mlvmreadchar(infile, 1);
							}
						}
			}
			c = ch1;
			do {
				if (c == '/') {
					if (mlvmpeekchar(infile) == '*') {
						break;
					}
				}
				*s++ =c;
			} while (chtype[(c=mlvmreadchar(infile, enterimago))] == SY);
			*s = '\0';
			break;
		default:
			break;
		}
	}  /* the first while */
	printtemp(outfile);
}


createfileif(FILE *infile, FILE *outfile)
{
	register char c;
	char *s;
	int enterimago=0, curpos=0, curstr=0;
	char tempstr[LINE_BUF],tempstr10[10][LINE_BUF]; /* will change to dynamic allocation later */
	while((c = mlvmreadchar(infile, enterimago)) != '\0')  /* end of file */
	{
		if(needtoprint==1) { printtemp(outfile); needtoprint=0;}
		while (chtype[c] == BS)
		{
			c = mlvmreadchar(infile,enterimago);

		}
		switch(mask_class_no(chtype[c])) {
		case mask_class_no(SY):
			s = buffer1;
			if (ch1 == '.' &&
					(chtype[mlvmpeekchar(infile)] == BS || chtype[mlvmpeekchar(infile)] == ENDF)) {
				mlvmreadchar(infile, enterimago);
				break;
			} else if (c == '/' && mlvmpeekchar(infile) == '*') { /* enter the comment */
					mlvmreadchar(infile, 1);       /* read the * symbol */
						while (!((c = mlvmreadchar(infile, 1)) == '*' && mlvmpeekchar(infile) == '/')
						         && chtype[c] != ENDF) ;
						mlvmreadchar(infile, 1); /* end of comment */
			}
			else if( c=='/' && mlvmpeekchar(infile)=='/')
			{
				mlvmreadchar(infile, 1);
				while( (c=mlvmreadchar(infile, 1))!='\n') ;
			}
			else { c=mlvmreadchar(infile, 0);}
			c = ch1;
			*s = '\0';
			break;
		default:
			break;
		}
	}  /* the first while */
	printtemp(outfile);
}

initread()
{
	COMMENT_NEST1 = 1;
	unread_cnt1 = 0;
	old_len1 = 0;
	line_pos1 = 0;
	lineno1 = 1;
}

void finddir(char *infilename)
{
	int namelen=0, findit=0, ii=0;
	strcpy(inputfiledir, infilename);
	namelen=strlen(inputfiledir);
	while(ii<=namelen && findit==0)
	{
		if(inputfiledir[namelen-ii]!='/')	ii++;
		else { findit=1; inputfiledir[namelen-ii+1]='\0';}
	}
	if(findit==0) strcpy(inputfiledir, "./");
}

int endbyimp(char *infilename)
{
	int filenamelen;
	filenamelen=strlen(infilename);
	if(!strcmp(infilename+strlen(infilename)-4, ".imp")) return 1;
	else return 0;
}

preprocessor(char *filename)  /* get rid of IMAGO directive, produce standard Prolog file */
{
	FILE *inputfile, *outputfile, *usermodule;
	/* first check the file with only one IMAGO inside, plus comment , without module definition */
	emptyusermodule=1;
	/*strcpy(tempfile, "./temp/");*/
	/*strcpy(tempfile, "./");*/
	strcpy(tempfile, "");
	strcat(tempfile, filename);
	strcat(tempfile, ".pl");
	if((inputfile=my_open(tempfile, "r"))==NULL)
	{
		/*strcpy(tempfile, "./temp/");*/
		/*strcpy(tempfile, "./");*/
		strcpy(tempfile, "");
		strcat(tempfile, filename);
		if(endbyimp(filename)!=1)
		{
			strcat(tempfile, ".imp");
			strcat(filename, ".imp");
		}
		if((inputfile=my_open(tempfile, "r"))==NULL)
		{
			printf("can not open any files associated with %s\n", filename);
			exit(-1);
		}
		else sourcefiletype=1;
	}
	else sourcefiletype=0;
	if(sourcefiletype==1)
	{
	/*outputfile=my_open("outimago", "w");*/
	/*strcpy(tempfile, "./temp/");*/
	/*strcpy(tempfile, "./");*/
	strcpy(tempfile, "");
	strcat(tempfile, filename);
	strcat(tempfile, ".if");
	/*usermodule=my_open("./temp/ifthenelse", "w");*/
	usermodule=my_open(tempfile, "w");
	createfileif(inputfile, usermodule);  /* create .if intermediate file */
	fclose(inputfile);
	fclose(usermodule);
	inputfile=my_open(tempfile, "r");
	/*usermodule=my_open("./temp/usermodule", "w");*/
	/*usermodule=my_open("./usermodule", "w");*/
	strcpy(tempfile,"");
	strcat(tempfile, filename);
	strcat(tempfile, ".umo");  /* for user module file */
	usermodule=my_open(tempfile, "w");
	createusermodule(inputfile, usermodule);
	initread();
	/*fclose(inputfile);*/
	fclose(usermodule);
	/*usermodule=my_open("./temp/usermodule", "r");*/
	if(GENFILE>=2)
	{
		/*usermodule=my_open("./usermodule", "r");*/
		usermodule=my_open(tempfile, "r");
		fseek(inputfile, 0, SEEK_SET);  /* goto the beginning of inputfile */
		finddir(tempfile);
		preprocessorimagoscan(inputfile, usermodule);
	}
	}
	if(sourcefiletype==0)
	{
			/*strcpy(tempfile, "./temp/");*/
			/*strcpy(tempfile, "./");*/
			strcpy(tempfile, "");
			strcat(tempfile, filename);
			strcat(tempfile, ".if");
			/*usermodule=my_open("./temp/ifthenelse", "w");*/
			usermodule=my_open(tempfile, "w");
			createusermodule(inputfile, usermodule);
			fclose(usermodule);
	}
}

cleaninterfile()
{
	char tempfile[LINE_BUF];
	char iffile[LINE_BUF];
	char commandstr[LINE_BUF];
	IMAGONAMENODE *temp;
	if(sourcefiletype==1)  /* imp file */
	{
		temp=imagonamehead;
		strcpy(iffile, fileargument);
	 	while(temp!=NULL)
		{
			/*strcpy(tempfile, "./");*/
			strcpy(tempfile, "");
			strcat(tempfile, iffile);
			strcat(tempfile, ".umo ");
			strcat(tempfile, iffile);
			/*strcat(tempfile, temp->filename);*/
			if(GENFILE==2)
			{
				strcat(tempfile, ".if ");
			}
			else if(GENFILE==3)
			{
				strcat(tempfile, ".if ");
				strcat(tempfile, temp->filename);
				strcat(tempfile, ".ima ");
			}
			else if(GENFILE==4)
			{
				strcat(tempfile, ".if ");
				strcat(tempfile, temp->filename);
				strcat(tempfile, ".ima ");
				strcat(tempfile, temp->filename);
				strcat(tempfile, ".ls ");
			}
		sprintf(commandstr, "rm -Rf %s", tempfile);
		if(GENFILE>=2) system(commandstr);
		temp=temp->next;
		}
	}
	else                  /* pl file */
	{
		/*strcpy(tempfile, "./");*/
		strcpy(tempfile, "");
		strcat(tempfile, fileargument);
		if(GENFILE==2)
		{
			strcat(tempfile, ".if ");
		}
		else if(GENFILE==3)
		{
			strcat(tempfile, ".if ");
			strcat(tempfile, fileargument);
			strcat(tempfile, ".ima ");
		}
		else if(GENFILE==4)
		{
			strcat(tempfile, ".if ");
			strcat(tempfile, fileargument);
			strcat(tempfile, ".ima ");
			strcat(tempfile, fileargument);
			strcat(tempfile, ".ls ");
		}
	}
		sprintf(commandstr, "rm -Rf %s", tempfile);
		if(GENFILE>=2) system(commandstr);
		temp=temp->next;
}


compilefile()
{
char *s;
FILE *fp;
IMAGONAMENODE *temp;
char tempfile[100];
char commandstr[100];
char s1[100];

	s=fileargument;
    if(GENFILE>=1)
    {
		preprocessor(s);
		if(sourcefiletype==1)
		{
		temp=imagonamehead;
		while(temp!=NULL)
		{
			if(GENFILE>=3)
			{
				/*strcpy(tempfile, "./");*/
				strcpy(tempfile, "");
				strcat(tempfile, temp->filename);
				strcat(tempfile, ".ima");
				havefail=0;
				mlvminternal_consult(temp, tempfile);
			}
			temp=temp->next;
		}
		temp=imagonamehead;
		while(temp!=NULL)
		{
			if(GENFILE>=4)
			{
				/*strcpy(tempfile, "./");*/
				strcpy(tempfile, "");
				strcat(tempfile, temp->filename);
				strcat(tempfile, ".ls");
		    	        sprintf(s1, "%s", MASMFILE );
				if((fp=fopen(s1,"r"))!=NULL)
				{
					sprintf(commandstr, "%s %s", s1, tempfile);
					printf("can not find assembler\n");
					fclose(fp);
				}
				else sprintf(commandstr, "%s %s", MASMFILE, tempfile);
				system(commandstr);
			}
			temp=temp->next;
		}
		return;
		}
/***********************lh100**************************/
		else return internal_consult(s);
	}
}

