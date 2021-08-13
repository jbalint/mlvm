/** $RCSfile: assembler.cc,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: assembler.cc,v 1.14 2003/03/25 13:39:25 gautran Exp $ 
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
#include <cstring>  // contains strchr

Assembler::Assembler(ifstream& in, ofstream& out, char* nm, 
			char* lb, int* pp, PROCREC* tp, PROCREC* rp): 
	inf(in), 
	outf(out),
	ops(OP_HASH_SIZE), 
	bts(BT_HASH_SIZE), 
	nms(NM_HASH_SIZE, nm, MAX_NAMES), 
	lbs(LB_HASH_SIZE, lb, MAX_LABELS, pp),
	name_base(nm),
	label_base(lb),
	code_base(pp),
	table_base(tp),
	proc_base(rp),
	cc(0),
	tc(0),
	pc(0),
 	line(1),
	imago(0),
	type(0)
{ }

int Assembler::my_atoi(char* c, int sz){
	int i = 0;
	
	for (int j = 0; j < sz; j++){
		if (isdigit(c[j])) i = i*10 + c[j] - '0';
		else break;
	}
	return i;
}
int Assembler::get_opcode(){
	char buf[MAX_LEN];
 	int loc;
 	char c = 0;

	// read an opcode. if a label is found, insert to lbs
	// return -1 if eof, otherwise return index of opcode_tab
	
	while(1){
	
		if (inf.eof()) return -1;
		
		inf.get(c);
		
		if ((int) c == 0) return -1;	
		else if (c == '/') new_line();
		else if (c == '\n') line++;
		else if (c != ' ' && c != '\t'){ 
		  //cout << "char = " << c << endl; 
			inf.seekg(-1, ios::cur);
			break;
		}
	}
  		
	inf >> buf;

	while (1) { // checking for labels
 	 			
		loc = strlen(buf) - 1;

		if (buf[loc] == ':'){	// if it is a label		
			buf[loc] = '\0';
			lbs.insert(buf, cc, line);
			/** GA - 03/02/03 - Label lines where misscounted. */
			while (!inf.eof()){
			    inf.get(c);
			    if( c == '\n' ) {line++; break;}
			    if( !isspace(c) ) {inf.unget(); break;}
			}  
			/** */
			inf >> buf;	// read a string again
		}
		else if (buf[0] == '/'){ // if it is a comment line
			new_line();
			inf >> buf;
		}
		else break;
	}

	loc = ops.lookup(buf);
	
	//	cout << " buf[0] = " << (int) buf[0] << endl;

	if (loc == -1){
		error(line, "Illegal opcode code ", buf);
	}
	
	//	cout << buf << " (" << loc << ") " << endl;
	
	return loc;
}

int Assembler::get_builtin(){
	char buf[MAX_LEN];
 	int loc;

	inf >> buf;

	//	cout << "get_builtin " << buf << " " << endl;

	loc = bts.lookup(buf);

	if (loc == -1){
		error(line, "Illegal builtin name ", buf);
	}

	/** We need to check the authorization first. */
	if( (type == I_STATIONARY) && (btin_tab[loc].legal & LEGAL_STATIONARY) ) {
	    /** Valid. */
	} else if( (type == I_WORKER) && (btin_tab[loc].legal & LEGAL_WORKER) ) {
	    /** Valid. */
	} else if( (type == I_MESSENGER) && (btin_tab[loc].legal & LEGAL_MESSENGER) ) {
	    /** Valid. */
	} else if( (type == I_MESSENGER) && (btin_tab[loc].legal & LEGAL_SYSMSGER) ) {
	    /** Valid. */
	} else {
	    /** Invalid. */
	    error( line, "Illegal builtin: ", buf );
	}
	/** All good to go. */

	
	//	cout << buf << " " << loc << endl;
	return loc;	

}

int Assembler::get_num(){
 	int loc;

 
	inf >> loc;
	
//	cout << "get_num " << loc << " ";
	
	return loc;

}

int Assembler::get_int(){
 	int loc;

	inf >> loc;
	
// 	cout << "get_int" << loc << " ";
	
	return make_int(loc);

}

int Assembler::get_con(){
	char buf[MAX_LEN];
	char *ptr = buf;
 	int loc, ignore = 0;

	inf >> buf;

	/** 03/02/02 GA - Check if the constant string is enclosed between single cote '\'' */
	if( buf[0] == '\'' ) { 	  /** putcon 'some long string' */
	  ptr++; /** Skip the '\'' */
	  loc = strlen(buf) - 1; /** Find the end of the string. */
	  if( buf[loc] != '\'' || loc == 0 ) {
	    loc++; /** Do not overwrite the last letter. */
	    /** Start the hunt for '\'' */
	    while( !inf.eof() ) {  
	      inf.get( buf[loc] );
	      if( buf[loc] == '\n' ) break;
	      else if( buf[loc] == '\\' ) ignore = 1;
	      else if( buf[loc] == '\'' && ignore == 0 ) break;
	      else if( buf[loc] == '\'' && ignore == 1 ) ignore = 0;
	      loc++;
	    }  
	    if( buf[loc] != '\'' ) {
	      warning(line, "missing terminating \' character", "");
	    }
	  } 
	  /** Clean up the closing '\'' */
	  buf[loc] = 0x00;
	}
	/** GA - END */

	//      cout << "get_con " << ptr << endl;;

	if (ptr[1] == '\0') 
		return make_con(ptr[0]); // single char

	loc = nms.lookup(ptr);
	
	
	return make_con(loc + ASCII_SIZE);

}

int Assembler::get_fun(){
	char buf[MAX_LEN];
 	int loc, i = 0, arity;
 	char* temp;
 	
 	inf >> buf;

	while(buf[i] && buf[i] != '/') i++;

	if (buf[i] == '\0') 
		error(line, "Illegal functor ", buf);
 
	buf[i] = '\0';   // break buf into two sub-strings: func, arity
	temp = buf + i + 1;
	arity = my_atoi(temp, strlen(temp));
	 
	if (i == 1) 	// single char functor
		return make_fun(buf[0], arity);
	
 	loc = nms.lookup(buf);
 	
// 	 cout << "get_fun " << buf << " FUN(" << loc << ") ";
 	
 	return make_fun(loc + ASCII_SIZE, arity);
 
}

int Assembler::get_label(){
	char buf[MAX_LEN];
 	int loc;
 	
 	inf >> buf;
 	if (buf[0] == '/' && buf[1] == '/') {
 		cerr << "Wrong Label " << buf << " at line " << line << endl;
 		exit(1);
 	}
 	
 	loc = lbs.lookup(buf, cc, line);
 	
 // 	cout << "get_label" << buf << "(" << loc << ") ";
 	
 	if (loc == -1) return 0;
 	else return loc;

}

void Assembler::put_code(int code) {
	if (cc >= MAX_CODE) 
		error(line, "Program too large", "");
//	cout << "put_code " << code << endl;
 	code_base[cc++] = code;
}
	
void Assembler::put_table_entry(int t, int loc){
	if (tc >= MAX_HASH_ENTRIES) 
		error(line, "Hash table run out", "");
	table_base[tc].term = t;
	table_base[tc++].loc = loc;
}

PROCREC Assembler::get_table_entry(int i){
	if (i >= tc)
		error(line, "No such entry in hash table", "");
	return table_base[i];
}

void Assembler::put_proc_entry(int t){

	if (cc >= MAX_CODE) 
		error(line, "Program too large", "");
 	if (pc >= MAX_PROC_ENTRIES) 
		error(line, "procedure table run out", "");
	proc_base[pc].term = t;
	proc_base[pc++].loc = cc + 1;
 	put_code(t);

	// check if the procedure is the imago entry

	if (t == imago) code_base[1] = cc; // refill the imago instruction's operand

}

int Assembler::get_prime(int s){
	int i = 3;
	s = (s % 2) ? s + 8  : s + 9; // make s an odd number, slight greater than required
	while(i < s/2){
		if (s % i == 0){
			i = 3;
			s += 2;
		}
		i += 2;
	}
	return s;
}

double Assembler::get_float(){
 	double dd;

	inf >> dd;
	
 //	cout << "get_float" << dd << " ";
	
	return dd;
}

void Assembler::put_hash_code(PROCREC e, int* pc, int prime){
int probe, addr, key;
int *pe;

	/* each table entry needs 2 cells: (key, loc) 
	   while key and loc
	   are to be filled. The addr gives the index of each table entry,
	   and pc is the starting address of the code table for hashing. */

	probe = 0;
	key = e.term;
	addr = key%prime;
//	cout << "put_hash_code, addr = " <<  addr << endl;
	while(probe < prime){
		pe = pc + addr*2;	// get entry (key) address
		if (*pe == 0){ // not occupied yet
			*pe = key;
			*(pe + 1) = e.loc; // assign entry (loc)
			return;
		}
		else if (*pe  == key){ // already there
			return;
		}
		else{ // need probing
			probe++;	// linear probing
			addr = (addr + 1) % prime;
//			cout << "collision: " << probe << endl;
		}
	}
}

 
int Assembler::get_table(){
	// a hashtable instruction consists of:
	// n a1 e1 .... an en v
	// it is translated as:
	// [prime, var_branch, hashing table].
	// where  each entry in hashing table is a PROCREC. i.e. {term, loc}
	// A hash-instruction has the address starting from [prime ...]
	// thus the actural hashing table is located by adding 2 locations
	// the prime is the size of the hashing table, it is determined
	// by this progrtam such that prime is the smallest prime number which
	// greater than n (n is the number of pairs of [ai, ei]'s).
	// we use linear probing to solve collisions.
	// keys are term values (either con-value or fun-value), they
	// are different indices to the symbolic (name) table.
	// The hasing function is simple, h(key) == key%prime.

 	int i, n, loc, term;	
 	
 	n = get_num();

	// read pairs into table_base[tc]

	tc = 0; // initialize temporary table
	for (i = 0; i < n; i++){
		term = get_con();
		if (!(loc = get_label()))
 			warning(line, "Hashing label not defined", "");
 		put_table_entry(term, loc);
 	}

	if (!(loc = get_label()))
 		warning(line, "Hashing label not defined", "");
 	
 	put_table_entry(0, loc);	// var branch
	return n;
}

void Assembler::put_table(int n){	
	int i, loc, prime;
	int* pc;
 
	// prepare hashing 

 	prime = get_prime(n);
  	
	loc = get_table_entry(n).loc;

	// init first two entries [prime, var_branch, ...

	put_code(prime);
	put_code(loc);

	// save hashing starting address

	pc = code_base + cc; 			
 
	// initialize hashing table, loc is the var branch at this moment

 	for (int i = 0; i < prime; i++){
		put_code(0); // (type, var_term)
		put_code(loc); // (type, var_branch)
  	}

	// insert [a,e] pairs into code area

	for (i = 0; i < n; i++){
		put_hash_code(get_table_entry(i), pc, prime);
	}
}
 	
 	

void Assembler::new_line(){
 	char c;
 
 	while (!inf.eof()){
 		inf.get(c);
  		if (c == '\n') break;
	}  
	
	line++;
}

void Assembler::start(){

	translate();
	codegen();  
}

void Assembler::translate(){
 	int op;
  	double d;
	int* pd = (int*) &d;

  	op = nms.lookup("[]");		// for con_nil system constant

//	cout << "in translation: " <<  endl;

 	if ((op = get_opcode()) == -1) exit(0);
	if ((type = opcode_tab[op].type) > I_MESSENGER) error(line,"Not an imago program", "");
 	put_code(opcode_tab[op].opcode);	// put imago instruction
	put_code(0);						// to be refilled when entry procedure is found
	imago = get_fun();					// get the entry procedure name xxx/1
 	
	// translate the rest
	
	while(1){
	
	  if ((op = get_opcode()) == -1) break;
	
	  if (is_instruction(opcode_tab[op].opcode)){
	      //	  	cout << "instruction: " << opcode_tab[op].name << " " 
	      // << opcode_tab[op].opcode<< endl;
	  	put_code(opcode_tab[op].opcode);
	  }
	  else {
	      //	  	cout << "directive : " << opcode_tab[op].name << endl;
	  	put_code(opcode_tab[op].opcode);
	  }	
	  	
	  switch (opcode_tab[op].type) {

		caseop(I_STATIONARY):
		caseop(I_WORKER):
		caseop(I_MESSENGER):
			error(line,"Too many imago in one file", "");
	  	
	  	caseop(I_O):
 	  		break;
	  		
		caseop(I_N):		// number
			put_code(get_num());
 			break;
			
		caseop(I_E):		// label
			put_code(get_label());
 			break;

		caseop(I_C):		// constant (int/chars)
			put_code(get_con());
 			break;

		caseop(I_F):		// functor
			put_code(get_fun());
 			break;

		caseop(I_B):		// builtin
  			put_code(get_builtin());
 			break;

		caseop(I_I):		// integer
			put_code(get_int());
 			break;

		caseop(I_P):		// procedure
			put_proc_entry(get_fun());
 			break;

		caseop(I_R):		// floating value
			d = get_float();
 			put_code(*pd);
			put_code(*(pd+1));
 			break;

		caseop(I_2N):
			put_code(get_num());
			put_code(get_num());
 			break;

		caseop(I_3N):
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
  			break;
 			
		caseop(I_4N):
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
 			break;
 			
 		caseop(I_5N):
			put_code(get_num());
			put_code(get_num());
 			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
  			break;

		caseop(I_6N):
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
 			break;

		caseop(I_7N):
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
 			break;

		caseop(I_8N):
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
 			break;

		caseop(I_NE):
			put_code(get_num());
			put_code(get_label());
 			break;

		caseop(I_NC):
			put_code(get_num());
			put_code(get_con());
 			break;

		caseop(I_NF):		// integer functor
			put_code(get_num());
			put_code(get_fun());
 			break;

		caseop(I_NI):
			put_code(get_num());
			put_code(get_int());
 			break;

		caseop(I_NR):
			put_code(get_num());
			d = get_float();
 			put_code(*pd);
			put_code(*(pd+1));
  			break;

		caseop(I_2NE):
			put_code(get_num());
			put_code(get_num());
			put_code(get_label());
 			break;

		caseop(I_2NF):
			put_code(get_num());
			put_code(get_num());
			put_code(get_fun());
 			break;

		caseop(I_NFE):
			put_code(get_num());
			put_code(get_fun());
			put_code(get_label());
 			break;
 			
		caseop(I_2NFE):
			put_code(get_num());
			put_code(get_num());
			put_code(get_fun());
			put_code(get_label());
 			break;

		caseop(I_3NE):
			put_code(get_num());
			put_code(get_num());
 			put_code(get_num());
			put_code(get_label());
 			break;

		caseop(I_3NFE):
			put_code(get_num());
			put_code(get_num());
			put_code(get_num());
			put_code(get_fun());
			put_code(get_label());
 			break;

		caseop(I_4E):
			put_code(get_label());
			put_code(get_label());
			put_code(get_label());
			put_code(get_label());
			break;
		
		caseop(I_N4E):
			put_code(get_num());
 			put_code(get_label());
			put_code(get_label());
			put_code(get_label());
			put_code(get_label());
			break;
	
		caseop(I_HTAB):		// hashtable		
			put_table(get_table());
 			break;	
			
 	  }
	  new_line();
//	cout << endl;
	}	
}

void Assembler::codegen(){

	// bytecode file format:
	// magic_num, imago_type, security_code
	// symbol_size, procedure_size, code_size 
	// symbol_table (each name is ended with a \n)
	// procedure_table,
	// code
 	
	int i, magic_num = 0, security_code = 0;
	
 	int symbol_size = nms.get_size();
	
	// Check if the imago entry procedure has been defined.
	if( code_base[1] == 0 ) {
	    error(line, "Imago entry procedure not defined", ""); 
	}

	// check if any unresolved labels
	
	if (lbs.check() == -1){
		error(line, "Labels not resolved", "");
	}
	
	// output magic_num, imago_type, security_code

	outf << magic_num << " " << type << " " << security_code << endl;

 	// output symbol_size (byte), procedure_size*{pterm, entry}, code_size*{type, code} 
	
	outf << symbol_size << " " << pc << " " << cc << endl << endl;

 	// output symbol_table (each string is ended with a \n)

	for (i = 0; i < symbol_size; i++){
		if (name_base[i] == '\0') outf << '\n';
		else outf << name_base[i];
	}
 
	// output procedure_table

	for (i = 0; i <  pc; i++){
		outf << proc_base[i].term << " " << proc_base[i].loc << " ";
		if (!((i+1) % 10)) outf << endl;
	} 
	outf << endl;
		
	// output byte code

	for (i = 0; i < cc; i++){
		outf << code_base[i] << " ";
		if (!((i+1) % 20)) outf << endl;
	} 
	outf << endl;
}

