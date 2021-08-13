/** $RCSfile: main.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: main.c,v 1.12 2003/03/25 21:05:08 hliang Exp $
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


#include <signal.h>
#include "builtins.h"
#include "parser.h"
#include "lib.h"

int GENFILE;          /* control for generating file */
int OPTIMVAL;         /* 0: unoptimization    1: do optimization */
int PRINTTAB;         /* 1: print the table and tree for debug */


FILE *outfile;        /* input/output stream numbers */
FILE *errormesg;
char *input_fname;             /* file to be reconsulted immediately */
extern char fileargument[];

void segmentfault()
{
	fprintf(errormesg, "Segmentation fault: no core dumped\n");
	fflush(errormesg);
	exit(1);
}

optionerror()
{
	fprintf(errormesg, "Options: imagoc [-i *.if <n>] ");
	fprintf(errormesg, "[-a  *.ima] [-s *.ls] [-o  *.lo]\n");
	fflush(errormesg);
	exit(2);
}

warning(s, arg1, arg2)
char *s;
int arg1, arg2;
{
	char buf[256];
	sprintf(buf, s, arg1, arg2);
	fprintf(errormesg, "Warning: %s\n", buf);
	fflush(errormesg);
}

errorabort(s, arg1, arg2)
char *s;
int arg1, arg2;
{
	char buf[256];
	sprintf(buf, s, arg1, arg2);
	fprintf(errormesg, "Error: %s\n", buf);
	fflush(errormesg);
	fprintf(errormesg, "Aborting\n");
	exit(3);
}

fatalerror(s)
char *s;
{
	fprintf(errormesg, "Fatal Error and Exit: %s\n",s);
	fflush(errormesg);
	exit(1);
}

main(argc, argv)
int argc;
char *argv[];
{
char *opt_p;
int i;
	errormesg = stderr;
	outfile = stdout;
   	signal(SIGSEGV, segmentfault);
	OPTIMVAL=0;
	PRINTTAB=0;
	if(argc==1) { printf("missing the source file name!\n"); exit(1); }
	if(argc==2) { printf("start compiling, wait...\n");strcpy(fileargument, argv[1]); GENFILE=4;}
	if(argc>2)
	{
		argc--;
		argv++;
		GENFILE=4;
		while (argc && *argv[0] == '-')
		{
			opt_p = argv[0];
			while(*(++opt_p))
			{
				switch(*opt_p) {
				case 'i':
					if (*(++opt_p)) optionerror();
					else {GENFILE=1/*; argc--; argv++*/; }
					goto NEXT_ARGU;
				case 'a':
					if (*(++opt_p)) optionerror();
					else {GENFILE=2;/*argc--; argv++;strcpy(fileargument, *argv);*/}
					goto NEXT_ARGU;
				case 's':
					if (*(++opt_p)) optionerror();
					else {GENFILE=3;/*argc--; argv++; strcpy(fileargument, *argv);*/}
					goto NEXT_ARGU;
				case 'o':
					if (*(++opt_p)) optionerror();
					else {GENFILE=4;/*argc--; argv++;strcpy(fileargument, *argv);*/}
					goto NEXT_ARGU;
				case 'p':
					if (*(++opt_p)) optionerror();
					else {OPTIMVAL=1;/*argc--; argv++;strcpy(fileargument, *argv);*/}
					goto NEXT_ARGU;
				case 't':
					if (*(++opt_p)) optionerror();
					else {PRINTTAB=1;/*argc--; argv++;strcpy(fileargument, *argv);*/}
					goto NEXT_ARGU;
				default: optionerror();
				}
			}
			NEXT_ARGU:
			argc--;
			argv++;
		}
		strcpy(fileargument, *argv);
	}
		fprintf(outfile, "\nIMAGOC Version 1.0\n");
		fprintf(outfile,"(c) IMAGO LAB -- University of Guelph\n");
		fflush(outfile);
		init_all();
		really_free_clauses();
		compilefile();
		cleaninterfile();
		fflush(stdout);
		exit(0);
}

