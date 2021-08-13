/** $RCSfile: main.cc,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: main.cc,v 1.8 2003/03/21 13:12:02 gautran Exp $ 
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
#include "assembler.h"
#include <unistd.h>

using namespace std;

string in_file = "";;
string out_file = "";

void print_term(char*, int) {
	return;
}

void report( const char *file, const int line, const char *m1, const char *m2 ) {
  cerr << file << ":" << line << ": " << m1 << m2 << endl;
}

void error(int line, const char* m1, const char* m2) {
   report(in_file.data(), line, m1, m2 );
   unlink(out_file.data());
   exit(EXIT_FAILURE);
}

void warning(int line, const char* m1, const char* m2) {
  cerr << "Warning: ";
  report(in_file.data(), line, m1, m2 );
}

char *get_progname(char *argv0)
{
#ifdef HAVE___PROGNAME
        extern char *__progname;

        return __progname;
#else
        char *p;

        if (argv0 == NULL)
                return "unknown";       /* XXX */
        p = strrchr(argv0, '/');
        if (p == NULL)
                p = argv0;
        else
                p++;
        return p;
#endif
}

int main(int argc, char *argv[]){
 	
 	ifstream inf;
 	ofstream outf;
	int i;
	char* base;

	
	cout << "MLVM Assembler 1.3" << endl;
	
 	if (argc < 2) {
	    cerr << "USAGE: " << get_progname(argv[0]) << " file_name" << endl;
 		exit(EXIT_FAILURE);
   	}
 
 	in_file = argv[1];
 	out_file = in_file;
 	
 	
 	if ((i = in_file.find(".ls")) == -1){
	    cerr << "USAGE: input file must have the extension .ls" << endl;
 		exit(EXIT_FAILURE);
  	}
 	
	inf.open(in_file.data());
	
	if (!inf){
		cerr << "cannot open input file: " << in_file << endl;
		exit(EXIT_FAILURE);
  	}
	
  	out_file[i + 2] = 'o';		// set output extension .lo
 	
	outf.open(out_file.data());
	
	if (!outf){
		cerr << "cannot open output file: " << out_file << endl;
		exit(EXIT_FAILURE);
 	}
	
	// memory allocation:
	// name_table[MAX_NAMES]
	// label_table[MAX_LABELS]
	// code_table[MAX_CODE]
	// hash_table[MAX_HASH_ENTRIES]
	// proc_table[MAX_PROC_ENTRIES]
	
	i = sizeof(char)*(MAX_NAMES + MAX_LABELS) + sizeof(int)*MAX_CODE
		+ sizeof(PROCREC)*(MAX_HASH_ENTRIES + MAX_PROC_ENTRIES);
	
	base = new char[i];
	
	
 	Assembler asmb(inf, outf, base, base + MAX_NAMES, 
 		(int*)(base+ MAX_NAMES +  MAX_LABELS),
 		(PROCREC*)(base+ MAX_NAMES + MAX_LABELS + sizeof(int)*MAX_CODE),
 		(PROCREC*)(base+ MAX_NAMES + MAX_LABELS + sizeof(int)*MAX_CODE
		+ sizeof(PROCREC)*MAX_HASH_ENTRIES));

		
	asmb.start();
	
	cout << "Bytecode program generated" << endl;
	
	inf.close();
	outf.close();

	return 0;
}


