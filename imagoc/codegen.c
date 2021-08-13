/** $RCSfile: codegen.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: codegen.c,v 1.12 2003/03/25 21:05:08 hliang Exp $
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
#include "builtins.h"
#include "parser.h"
#include "lib.h"

typedef struct imagonamenode{
	char* filename;
	struct imagonamenode *next;
	int imagotype;
} IMAGONAMENODE;

FILE *outlsfile;
char filenamearr[LINE_BUF];


extern int OPTIMVAL;
extern HASH_CLAUSE_NODE_POINTER hash_imago_table[];

extern int STYLE_CHECK_FLAG;

extern CLASUEPOINTER imago_first_clause();
FILE *my_open();

char* getnamefromdir(char* infile)
{
	int namelen=0, findit=0, ii=0, jj=0;
	strcpy(filenamearr, "");
	namelen=strlen(infile);
	while(ii<=namelen && findit==0)
	{
		if(infile[namelen-ii]!='/')	ii++;
		else { findit=1; }
	}
	if(findit==0) return infile;
	else
	{
		ii--;
		while(ii!=0)
		{
			filenamearr[jj++]=infile[namelen-ii];
			ii--;
		}
		filenamearr[jj++]='\0';
		return filenamearr;
	}
}


compile_mlvm(char clobber,  IMAGONAMENODE *inimago , char *infilename)
{
int i, j, k;
char *s;
int count;
CLAUSE *r, *r2, *last_compiled_clause, dummy;
int compiled_flag;
int createopenfile=1;
char tempfile[100];
	/*strcpy(tempfile, "./");*/
	strcpy(tempfile, "");
	if(inimago!=NULL)
	{
	strcat(tempfile, inimago->filename);
	strcat(tempfile, ".ls");
	outlsfile=my_open(tempfile, "w");
	/*if(inimago->imagotype==1) fprintf(outlsfile, "   stationary  %s/1\n", inimago->filename);
	if(inimago->imagotype==2) fprintf(outlsfile,"   worker  %s/1\n", inimago->filename);
	if(inimago->imagotype==3) fprintf(outlsfile, "   messenger  %s/1\n", inimago->filename);*/
	if(inimago->imagotype==1) fprintf(outlsfile, "   stationary  %s/1\n", getnamefromdir(inimago->filename));
	if(inimago->imagotype==2) fprintf(outlsfile,"   worker  %s/1\n", getnamefromdir(inimago->filename));
	if(inimago->imagotype==3) fprintf(outlsfile, "   messenger  %s/1\n", getnamefromdir(inimago->filename));
	}
	else
	{
		strcat(tempfile, infilename);
		strcat(tempfile, ".ls");
		outlsfile=my_open(tempfile, "w");
	}
	for (i = 1; i < getmaxfunc(); i++) { /*** procedure i ***/

		if (!(r = imago_first_clause(i))) continue;
		compiled_flag = FALSE;
		if (!imago_compiled(i)) { /*** static procedure ***/
			if(inimago!=NULL)
			{
				if(OPTIMVAL==1)	mlvmcodegenimagooptim(hash_imago_table[i+1], inimago, createopenfile);
				else mlvmcodegenimago(hash_imago_table[i+1], inimago, createopenfile);
				if(createopenfile==1) createopenfile=0;
				hash_imago_table[i+1]=NULL;
				/* get rid of these clause's hash_imago_table[], so that it can compile the same predicate in
			                                 other IMAGO file */
			}
			else
			{
				if(OPTIMVAL==1)	mlvmcodegenimagooptim(hash_imago_table[i+1], NULL, outlsfile);
				else mlvmcodegenimago(hash_imago_table[i+1], NULL, outlsfile);
			}
		}
	}

	fclose(outlsfile);
}


