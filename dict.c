#include "dict.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

node** table = NULL;
int M = 8;
int size = 0;
node DELETED = { NULL,NULL };	

static int64_t hash(char *key, size_t len){
	int a = 33;
	int64_t h = 1504;
	for (int i=0 ; i < len ; i++)
		h = ((h*a) + key[i]) % M;
	return h;
}

static int64_t hash2(char *key, size_t len){
	int a = 31;
	int64_t h = 1809;
	for (int i=0 ; i < len ; i++)
		h = ((h*a) + key[i]) % M;
	return ((h<<1) | 1)%M;
}

void alloc_dict() {
	table = malloc(sizeof(node*)*M);
	if (table==NULL) exit(EXIT_FAILURE);
	for (int i=0;i<M;i++){
		table[i] = NULL;
	}
}

void expand(){
	int start = M;
	M*=2;
	table = realloc(table,sizeof(node*)*M);
	if (table==NULL) exit(EXIT_FAILURE);
	for (int i=start;i<M;i++) table[i] = NULL;
}

void shrink(){
	M/=2;
	table = realloc(table,sizeof(node*)*M);
	if (table==NULL) exit(EXIT_FAILURE);
}

void dealloc_dict(){
	if (table==NULL) return;
	for (int i=0;i<M;i++){
		if (table[i]!=NULL && table[i]!=&DELETED){
			free(table[i]->key);
			free(table[i]->val);
			free(table[i]);
		} 
	}
	free(table);
}

int insert(char* key, char* value){
	int status;
	node** n = search(key,&status);
	if (status) {
		return 1;
	}
	*n = malloc(sizeof(node));
	if (!(*n)) return 1;
	size_t key_len = strlen(key)+1;
	size_t value_len = strlen(value)+1;
	(*n)->key = malloc(key_len);
	(*n)->val = malloc(value_len);
	if (!((*n)->key) || !((*n)->val)) exit(EXIT_FAILURE);
	memcpy((*n)->key,key,key_len);
	memcpy((*n)->val,value,value_len);
	size++;
	if (((double)size)/M > 0.66) expand();
	return 0;
}

node** search(char* key, int* status){
	size_t len = strlen(key);
	int64_t index = hash(key,len);
	int64_t jump = hash2(key,len);
	int64_t free = -1;
	*status = 0;
	if (table[index]==NULL){
		return &table[index];
	}
	while (table[index]!=NULL){
		if (table[index]==&DELETED) free = index;
		else if (memcmp(table[index]->key,key,len)==0){
			*status = 1;
			return &table[index];
		}
		index = (index+jump)%M;
	}
	return free!=-1? &table[free] : &table[index];
}

int delete(char* key){
	int status = 0;
	node** n = search(key,&status);
	if (status){
		free((*n)->key);
		free((*n)->val);
		free((*n));
		*n = &DELETED;
		size--;
		return 0; 
	 }
	 return 1;
}

void debug_all(){
	for (int i=0;i<M;i++){
		if (table[i]==NULL) printf("%d FREE\n",i);
		else if (table[i]==&DELETED) printf("%d DELETED\n",i);
		else printf("%d %s - %s\n",i,table[i]->key,table[i]->val);
	}
}

