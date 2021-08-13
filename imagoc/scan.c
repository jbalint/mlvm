/** $RCSfile: scan.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: scan.c,v 1.6 2003/03/25 21:05:08 hliang Exp $
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
#include <setjmp.h>
#include "builtins.h"
#include "parser.h"
#include "lib.h"

#define LINE_BUF	    (511)          /* max line length */


#define class(bits, no) ((bits << 4)| no)
#define CLASS_NO_MASK   (0xF)
#define mask_class_no(x)		(x)
#define mask_class_bits(x)		((x) & ~CLASS_NO_MASK)
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

char *cur_filename;

char ch,close_quote;
int tokentype;
char tokenchar;
IMAGOTERM *tokenpointer;
static char buffer[LINE_BUF];
int COMMENT_NEST = 1;
static char unread_buf[LINE_BUF];
static int unread_cnt = 0;
static char line_buf[LINE_BUF];
/*static int old_len = 0, line_pos = 0;
static int 	lineno = 1;*/
int old_len = 0, line_pos = 0;
int lineno = 1;

int comma_flg = FALSE;   /* ',' is a functor, not arg separator */
int bar_flg = FALSE;     /* TRUE in [A | B], otherwise treated as functor */

/*----------------------------------------------------- ascii ---------------*/

int ascii_classes[] = {

/* nul soh stx etx eot enq ack bel  bs  ht  nl  vt  np  cr  so  si */
    ENDF, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS,

/* dle dc1 dc2 dc3 dc4 nak syn etb can  em sub esc  fs  gs  rs  us */
    BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS, BS,

/*  sp   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   /  */
    BS, SL, DC,  SL, LC, SL, SY, QT, BK, BK, SY, SY, SL, SY, SY, SY,

/*  0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    N,  N,  N,  N,  N,  N,  N,  N,  N,  N, SY, SL, SY, SY, SY, SY,

/*  @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
   SY, UC, UC, UC, UC, UC, UC, UC, UC, UC, UC, UC, UC, UC, UC, UC,

/*  P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
   UC, UC, UC, UC, UC, UC, UC, UC, UC, UC, UC, BK, SY, BK, SY, UL,

/*  `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
   SY, LC, LC, LC, LC, LC, LC, LC, LC, LC, LC, LC, LC, LC, LC, LC,

/*  p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~  del */
   LC, LC, LC, LC, LC, LC, LC, LC, LC, LC, LC, SY, SL, SY, SY,  BS };


/*----------------------------------------------------- ebcdic --------------*/

int *chtype = ascii_classes;

char peekchar();
char readchar();

extern struct HASH_TAB_NODE *tok_node;
extern jmp_buf err_getimagoterm;
extern char *cons_str;
extern IMAGOTERM *lookuimago_var();
extern IMAGOTERM *lookuphashtab();
extern IMAGOTERM *create_real();
extern IMAGOTERM *create_pt_emptylist();
extern struct HASH_TAB_NODE *hnode_emptylist;

char readchar() /*** returns '\0' on EOF ***/
{
register int c;
	if (unread_cnt > 0) c = unread_buf[--unread_cnt];
	else if ((c = getc(cur_stream)) == EOF)  c = '\0';
	if (c == '\n') {
		lineno++;
		old_len = line_pos;
		line_pos = 0;
	} else
	if (line_pos < LINE_BUF) line_buf[line_pos++] = c;
	else {
		old_len = line_pos;
		line_pos = 0;
		line_buf[line_pos++] = c;
	}
	ch = c;
	return c;
}

unreadchar(c) /*** NULL for EOF ***/
register char c;
{
	if (c != '\0') {
		if (unread_cnt >= LINE_BUF) fatalerror("too many pushbacks");
		unread_buf[unread_cnt++]=c;
		if (c == '\n') lineno--;
		if (line_pos) line_pos--;
	}
}

char peekchar() /*** just look, not touch ***/
{
register char c1, c2;
	c1 = ch;
	c2 = readchar();
	unreadchar(c2);
	ch = c1;
	return c2;
}

get_name(s) /*** stores a NAME in s, NAME is either variable or symbolic ***/
register char *s;
{
register char c;
register int hash_sum;
	hash_sum = 0;
	c = ch;
	do {
		*s++ = c;
		hash_sum += c;
		c = readchar();
	} while (chtype[c] & mask_class_bits(UC | UL | LC | N));
	*s = '\0';
	return hash_sum;
}

imagoscan()
{
register char c;
char *s;
double tens, intpart, fracpart, num;
int sign = 1, esign, expart;
int level;
int hash_sum;

START:
	c = ch;
	while (chtype[c] == BS) c = readchar();

	switch(mask_class_no(chtype[c])) {

	case mask_class_no(UC):
	case mask_class_no(UL):
		tokenpointer = lookuimago_var(buffer, get_name(buffer));
		return(tokentype = VAR);
	case mask_class_no(LC):
		tokenpointer = lookuphashtab(buffer, get_name(buffer));
		return(tokentype = NAME);
	case mask_class_no(SY):
		s = buffer;
		if (ch == '.' &&
				(chtype[peekchar()] == BS || chtype[peekchar()] == ENDF)) {
			readchar();
			tokenchar = '.';
			return(tokentype = PUNC);
		} else if (c == '/' && peekchar() == '*') {
			level = 0;
		START_COMMENT:
				readchar();
				if (COMMENT_NEST) level++;
			WITHIN_COMMENT:
				if (COMMENT_NEST)
					while ((c = readchar()) != '/' &&
						c != '*' && chtype[c] != ENDF);
				else
					while ((c = readchar()) != '*' &&
						chtype[c] != ENDF);

				if (ch == '*' && peekchar() == '/') {
					readchar();
					readchar();
					if (!COMMENT_NEST || --level == 0) goto START;
				} else if (COMMENT_NEST && ch == '/' && peekchar() == '*')
					goto START_COMMENT;
				else if (chtype[ch] == ENDF)
					fatalerror("unexpected end of file in comment");
				goto WITHIN_COMMENT;
		}
		c = ch;
		hash_sum = 0;
		do {
			if (c == '/') {
				if (peekchar() == '*') {
					break;
				}
			}
			*s++ =c;
			hash_sum += c;
		} while (chtype[(c=readchar())] == SY);
		*s = '\0';
		tokenpointer = lookuphashtab(buffer, hash_sum);
		tokentype = NAME;
		return(tokentype = NAME);
	case mask_class_no(N):
		intpart = fracpart = 0;
		expart = 0; esign = 1;
		c = ch;
		do {
			intpart = intpart*10+(c-'0');
		} while (chtype[(c = readchar())] == N);
		if (c == '.' && chtype[peekchar()] == N) {
			tens = 0.1;
			c = readchar();
			do {
				fracpart += tens*(c-'0');
				tens *= 0.1;
			} while (chtype[(c = readchar())] == N);
		}
		if (c == 'e') {
			if (readchar() == '-') {
				esign = -1;
				readchar();
			} else {
				if (ch == '+') readchar();
				esign = 1;
			}
			if (chtype[ch] == N)
				do
					expart = expart*10+(ch-'0');
				while (chtype[readchar()] == N);
		}
		num = sign*(intpart+fracpart)*pow(10.0,(double) esign*expart);
		tokenpointer = create_real(num);
		return(tokentype = REAL);
	case mask_class_no(QT):
	case mask_class_no(DC):
		c = close_quote = ch;
		hash_sum = 0;
		for (s = buffer; (c = readchar()) != close_quote && chtype[c] != ENDF;
			 	*s++ = c, hash_sum += c);
		*s='\0';
		readchar();
		if (chtype[c] == ENDF)
			error_syntax("unexpected end of file within quotes");
		else { /* strings disabled - converted to literals */
			tokenpointer=lookuphashtab(buffer, hash_sum);
			return(tokentype = NAME);
		}
	case mask_class_no(SL):
		if (c == '%') {
			while ((c=readchar()) != '\n' && (chtype[c] != ENDF));
			if (c == '\n') {
				readchar();
				goto START;
			} else return(ENDFILE);
		}
		readchar();
		tokenchar = c;
		if (c == ',' && comma_flg) return(tokentype=PUNC);
		if (c == '|' && bar_flg) return(tokentype=PUNC);
		buffer[0]=c;
		buffer[1]='\0';
		tokenpointer=lookuphashtab(buffer, (int) c);
		return(tokentype=NAME);
	case mask_class_no(BK):
		tokenchar = c;
		c = readchar();
		if (tokenchar == '[' && c == ']') {
			readchar();
			tokenpointer = create_pt_emptylist();
			tok_node = hnode_emptylist;
			return(tokentype = NAME);
		}
		return(tokentype = PAREN);
	case mask_class_no(ENDF):
		return(tokentype = ENDFILE);
	default:
		error_syntax("illegal character");
		return 0; /* quiet gcc */
	}
}

is_punc(punc)
char punc;
{
	if (tokentype == PUNC && tokenchar == punc)
		return(TRUE);
	else	return(FALSE);
}

error_syntax(s) /*** if error, jump to getimagoterm in parser ***/
char *s;
{
int i;
	if (line_pos == 0 && old_len > 0) {
		printf("Syntax error, %s:%d : %s\n", cur_filename, lineno - 1, s);
		for (i = 0; i < old_len; i++) putchar(line_buf[i]);
	} else {
		printf("Syntax error, %s:%d : %s\n",cur_filename,lineno,s);
		for (i = 0; i < line_pos; i++) putchar(line_buf[i]);
	}
	printf("<--\n");
	while (!(tokentype==PUNC && tokenchar=='.') && tokentype != ENDFILE) imagoscan();
	longjmp(err_getimagoterm,TRUE);
}

stringsum(s)
register char *s;
{
register int c;
	c = 0;
	while (*s) c+= *s++;
	return c;
}

readline(s, n)
char *s;
int n;
{
int i;

	n--;
	readchar();
	for (i=0; ch != '\n' && i < n; i++) {
		*s++ = ch;
		readchar();
	}
	if (i == n) {
		while (ch != '\n' && ch)
			readchar();
	}
	*s = '\0';
}

the_lineno()
{
	if (ch == '\n') return lineno-1;
	else return lineno;
}

char *the_filename()
{
	return cur_filename;
}
