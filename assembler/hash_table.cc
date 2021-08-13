/** $RCSfile: hash_table.cc,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: hash_table.cc,v 1.6 2003/03/21 13:12:02 gautran Exp $ 
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
	/*             hash_table.cc                    */
	/************************************************/

#include "hash_table.h"
#include "opcode.h"
#include "builtins.h"
#include <cstring>  // contains strchr

HashNode* HashNode::pool= 0;

void* HashNode::operator new(size_t bytes){
	HashNode* p;
	
	if (bytes != sizeof(HashNode)){
		error(0, "HashNode::wrong size", "");
 	}
	if (!pool){
		pool = new HashNode[MAX_NODES];
		if (!pool) {
			error(0, "HashNode::run out space", "");
		}
		for (p = pool; p < pool + MAX_NODES  - 1; p++){
			p->next = p + 1;
		}
		p->next = NULL;
	}
	p = pool;
	pool = pool->next;
	return p;
}

void HashNode::operator delete(void *p){
	((HashNode*) p)->next = pool;
	pool = (HashNode*) p;
}

HashTable::HashTable(int sz){
	int i;
	hp = (HashNode**) new int[size = sz];
	for (i = 0; i < sz; i++)
		hp[i] = NULL;
}

int HashTable::hash(const char* key) const{
	unsigned int h;
	for (h = 0; *key; key++) h = ((h * h) << 2) + *key;
	if ((int) h >= 0) return (int) (h % size);
	else return (-(int)h) % size;
}

int HashTable::lookup(const char* key){
	int h;
	HashNode* cur;
	
	// return -1 if can not find key
	
	cur = hp[h = hash(key)];
	
	while (cur != NULL){
 	 	if (!strcmp(cur->name, key)){
	 		return cur->val;
	 	}
 
 		cur = cur->next;
	 }
 
 	 return -1;
}

int HashTable::insert(const char* key, int val){
	int h;
	HashNode* cur;
	
	// return -1 if key has already existed
	
	cur = hp[h = hash(key)];
	
	while (cur != NULL){
 	 	if (!strcmp(cur->name, key)) return -1;
 		cur = cur->next;
	}
	 
	 // create a new hash node
	 
	 cur = new HashNode(key, val);
  	 cur->next = hp[h];
 	 hp[h] = cur;
 	 return 1;
}

InstHashTable::InstHashTable(int sz):HashTable(sz){
	int i;
	 
	for (i = 0; opcode_tab[i].name; i++){
		insert(opcode_tab[i].name, i);
	}
};

BtinHashTable::BtinHashTable(int sz):HashTable(sz){
	int i;

	for (i = 0; btin_tab[i].name; i++) {
	    insert( btin_tab[i].name, i );
	}
};
		
int NameHashTable::lookup(const char* key){

/* NameHashTable lookup function will do the insert operation if the key is a 
   new one. It returns the offset the key is located at the name table */
	
	int h, len;
	HashNode* cur;
	char* new_key;
	
	cur = hp[h = hash(key)];
	
	while (cur != NULL){
	 	if (!strcmp(cur->name, key)){
	 		return cur->val;
	 	}
  		cur = cur->next;
	}
	
	// create a new hash node
	 
	 len = strlen(key) + 1;
	 
	 if (pos + len > max_names)
	 	error(cur->line,"NameHashTable::too many names", "(lookup)"); 
	 	
	 new_key = names + pos;
	 
	 strcpy(new_key, key);
	 
	 cur = new HashNode(new_key, pos);
	 count++;
  	 cur->next = hp[h];
 	 hp[h] = cur;
 	 pos += len;
	 return cur->val;	 	 
}

int LabelHashTable::lookup(const char* key){
 	error(0,"LabelHashTable::use lookup(key, pos) instead", "");
	return 0;
}

int LabelHashTable::lookup(const char* key, int loc, int ln){
	int h, len;
	HashNode* cur;
	char* new_key;
	
	cur = hp[h = hash(key)];
	
	while (cur != NULL){
 	 	if (!strcmp(cur->name, key) && (cur->val >= 0)){	 
	 		  return  cur->val;
	 	}
  		cur = cur->next;
	 }
 
	 /* create a new requesting hash node: this node will be
	    refilled later when the actual label is met. The loc
	    gives the offset from the codebase. */
	 
	 len = strlen(key) + 1;
	 
	 if (pos + len > max_labels)
	 	error(ln, "LabelHashTable::too many labels", "(lookup)"); 
	 	
	 new_key = labels + pos;
	 
	 strcpy(new_key, key);
	 pos += len; 
	 	 
	 cur = new HashNode(new_key, -loc, ln);
  	 cur->next = hp[h];
 	 hp[h] = cur;
 	 return -1;
 
}

int LabelHashTable::insert(const char* key, int loc, int ln){
	int h, len;
	HashNode *cur, *prev;
	char* new_key;
	 
	 // create a new hash node
	
	len = strlen(key) + 1;
	 
	if (pos + len > max_labels)
	 	error(ln, "LabelHashTable::too many labels", "(insert)"); 
	 	
	new_key = labels + pos;
	 
	strcpy(new_key, key);
	pos += len; 
	 	
	if ((cur = hp[h = hash(key)]) == NULL){
	 	cur = new HashNode(new_key, loc, ln);
  	 	cur->next = hp[h];
 	 	hp[h] = cur;
 	 	return 1; 
	}
	
	// otherwise, fix requests and check duplication
	
	// First create a label node

	prev = new HashNode(new_key, loc);
  	prev->next = cur;
 	hp[h] = prev;
 	 	
 	 // Then, we search for requests
 	 
 	while (cur){
	 	if (!strcmp(cur->name, key)){
	 		if (cur->val >= 0){
			  warning(ln, "LabelHashLabel::label redefined: ", key);
	 		  	error(cur->line, "(Location of previous definition).","");
	 		 	return -1;
	 		}
	 		// fix a request
	 		
	 		pp[-cur->val] = loc; // negative loc will be resolved by loader
	 		
	 		// remove req-node and repair links
	 		
	 		prev->next = cur->next;
	 		delete(cur);
	 		cur = prev->next;
		}
		else{
			prev = cur;
			cur = cur->next;		 
	 	}
	}
 	return 1;
}

int LabelHashTable::check(){
	int i, v = 1;
	HashNode *cur;
	
	for (i = 0; i < size; i++){
		cur = hp[i];
		while (cur != NULL){
			if (cur->val < 0){
				warning(cur->line, cur->name, " not found");
				v = -1;
			}
			cur = cur->next;
		}
	}
	return v;
}
  	

