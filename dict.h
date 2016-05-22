#ifndef _MEM_DICT_H
#define _MEM_DICT_H

#include <inttypes.h>
#include <stdio.h>

typedef struct node {
	char* key;
	char* val;
} node;

void alloc_dict();
void dealloc_dict();
int insert(char* key, char* value);
node** search(char* key, int* status);
int delete(char* key);


void debug_all();
#endif
