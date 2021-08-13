/** $RCSfile: assembler.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: assembler.h,v 1.10 2003/03/21 13:12:02 gautran Exp $ 
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
//#include "system.h"
#include "opcode.h"
#include "builtins.h"
#include "hash_table.h"

#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <string>
#include <fstream>
#include <stddef.h>

void error(int, const char* , const char*);
void warning(int, const char*, const char* );


using namespace std;

class Assembler{
	ifstream&  inf;
	ofstream&  outf;
	
	InstHashTable 	ops;
	BtinHashTable 	bts;
	NameHashTable	nms;
	LabelHashTable 	lbs;
	
	char* name_base;
	char* label_base;
	int* code_base;
	PROCREC* table_base;
	PROCREC* proc_base;

	int  cc;		// program count
	int  tc;		// hash table count, temporary use (coded in code_base)
	int  pc;		// procedure table count
 	int line;		// program line count
	int imago;		// imago entry procedure
	int type;		// imago type

	int my_atoi(char*, int);	
	int get_opcode();
	int get_builtin();
	int get_num();
	int get_int();
	int get_con();
	int get_fun();
	int get_label();
 	int get_table();
 	PROCREC get_table_entry(int);
 	int get_prime(int);
	double get_float();

	void put_code(int);
	void put_table_entry(int, int);
	void put_proc_entry(int);
	void put_table(int);
	void put_hash_code(PROCREC, int*, int);

	int is_instruction(int i){
		return (i < LVM_PROCEDURE);
	} 
	
	void new_line();
	
	void translate();
	void codegen();
	

public:
	Assembler(ifstream& in, ofstream& out, char* nm, 
		  char* lb, int* pp, PROCREC* tp, PROCREC* rp);
	
	void start();
};
