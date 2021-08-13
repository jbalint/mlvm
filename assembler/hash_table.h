/** $RCSfile: hash_table.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: hash_table.h,v 1.7 2003/03/21 13:12:02 gautran Exp $ 
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

	/************************************************/
	/*	 	hash_table.h			*/
	/************************************************/

#ifndef HASH_TABLE_HH
#define HASH_TABLE_HH

//#include "system.h"
#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <string>
#include <fstream>
#include <stddef.h>

void error(int, const char* , const char*);
void warning(int, const char*, const char* );


using namespace std;
	
class HashTable;
class NameHashTable;
class LabelHashTable;

class HashNode {
	
	friend class HashTable;
	friend class NameHashTable;
	friend class LabelHashTable;
		
	const char *name;		// name of this node
	int  val;		// index/opcode of builtin/instruction
 	HashNode *next;		// link
 	int line;

public:
	static HashNode* pool;  
	HashNode(){};
	HashNode(const char* nm, int vl): name(nm), val(vl), line(0){};
 	HashNode(const char* nm, int vl, int ln): name(nm), val(vl), line(ln){};
 	
 	void* operator new(size_t bytes);
 	void operator delete(void* p);

};


class HashTable {

protected:

	HashNode** hp;
	int size;

	int hash(const char* key) const;

public:
	HashTable(int sz);
	virtual int lookup(const char* key);
	virtual int insert(const char* key, int val);	

};
 
class InstHashTable : public HashTable{
public:
	InstHashTable(int sz);
};	

class BtinHashTable : public HashTable{
public:
	BtinHashTable(int sz);
};

/****************************************************************/
/* NameHashTable will be used by the assembler.                 */
/* The assember will create a table by:                         */
/*	NameHashTable(size, name_tab, max)	                    */
/* and use lookup(key) only.                                    */
/* which returns the offset of the key in  name_tab, this       */
/* offset will be used as the value of strings and functors     */
/****************************************************************/


class NameHashTable : public HashTable{
	char* names;
	int count;
	int max_names;
 	int pos;
public:
	NameHashTable(int sz, char* nm, int mx):
		HashTable(sz), 
		names(nm), 
		count(0), 
		max_names(mx), 
 		pos(0) 
		{};
	
 	int lookup(const char* key);
 	int get_count(){ return count;}
	int get_size(){ return pos;}
};

/****************************************************************/
/* LabelHashTable will be used by the assembler.                */
/* the assember will create a table by:                         */
/*	NameHashTable(size, label_tab, max, pp)                 */
/* where label_tab is a char array for storing labels and       */
/* pp is the program code base pointer for fixing labels.       */
/* and use insert(key, loc) to insert a new label and its pos   */
/* if the label exists, error. 	if there are requesting nodes   */
/* for this label, all these nodes will be fixed and removed.   */
/*                                                              */
/* lookup(key, loc) will be used for searching a label.         */
/* it returns 1 if pp[loc] is set, otherwise, it makes a        */
/* requesting node (negative pos) in the hash table.            */
/****************************************************************/

class LabelHashTable : public HashTable{
	char* labels;
	int count;
	int max_labels;
	int* pp;
	int pos;
public:
	LabelHashTable(int sz, char* lb, int mx, int* p):
		HashTable(sz), 
		labels(lb), 
		count(0), 
		max_labels(mx), 
		pp(p), 
		pos(0)
		{};
	int lookup(const char* key);
	int lookup(const char* key, int val, int ln);
	int insert(const char* key, int val, int ln);	
	int check();
};


#endif
	 
	 	  	
