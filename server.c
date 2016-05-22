#define _GNU_SOURCE
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include "dict.h"
#define ERR(source) (perror(source),\
                fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                dealloc_dict(),\
                exit(EXIT_FAILURE))
#define BACKLOG 3

volatile sig_atomic_t work = 1;

static ssize_t bulk_read(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
			c=TEMP_FAILURE_RETRY(read(fd,buf,count));
			if(c<0) return c;
			if(0==c) return len;
			buf+=c;
			len+=c;
			count-=c;
	}while(count>0);
	return len ;
}
static ssize_t bulk_write(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
			c=TEMP_FAILURE_RETRY(write(fd,buf,count));
			if(c<0) return c;
			buf+=c;
			len+=c;
			count-=c;
	}while(count>0);
	return len ;
}

static int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
			return -1;
	return 0;
}

static void sigint_handler(int signal){
	work = 0;
}

int make_socket(int domain, int type){
	int sock;
	sock = socket(domain,type,0);
	if(sock < 0) ERR("socket");
	return sock;
}
int bind_inet_socket(uint16_t port,int type){
	struct sockaddr_in addr;
	int socketfd,t=1;
	socketfd = make_socket(PF_INET,type);
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR,&t, sizeof(t))) ERR("setsockopt");
	if(bind(socketfd,(struct sockaddr*) &addr,sizeof(addr)) < 0)  ERR("bind");
	if(SOCK_STREAM==type)
			if(listen(socketfd, BACKLOG) < 0) ERR("listen");
	return socketfd;
}
int add_new_client(int sfd){
	int nfd;
	if((nfd=TEMP_FAILURE_RETRY(accept(sfd,NULL,NULL)))<0) {
			if(EAGAIN==errno||EWOULDBLOCK==errno) return -1;
			ERR("accept");
	}
	return nfd;
}

void handle_request(int client){
	int32_t length;
	ssize_t size;
	if((size=bulk_read(client,(char*)&length,sizeof(int32_t)))<=0) return;
	if (size!=sizeof(int32_t)) return;
	char* buffer = malloc(length);
	if (!buffer) return;
	if((size=bulk_read(client,buffer,length))<=0) return;
	if (size!=length) return;
	int8_t type = -1;
	memcpy(&type,buffer,sizeof(int8_t));
	switch(type){ 
		case 0: // INSERT
		{
			int32_t val_start = 0;
			memcpy(&val_start,buffer+sizeof(int8_t),sizeof(int32_t));
			char* key = buffer+sizeof(int8_t) +sizeof(int32_t);
			char* val = buffer+val_start;
			int8_t ret = (int8_t)insert(key,val);
			
			if (bulk_write(client,(char*)&ret,sizeof(int8_t))<0 &&errno!=EPIPE) ERR("write");
			
			debug_all();
			break;
		}
		case 1: // GET
		{
			char* key = buffer+sizeof(int8_t);
			int status = 1;
			node** n = search(key,&status);
			printf("Search status: %d\n",status);
			if (status){
				printf("%s - %s\n",(*n)->key,(*n)->val);
				
				char* response = malloc(strlen((*n)->val)+1+sizeof(int32_t)+sizeof(int8_t));
				if (!response) break;
				*((int32_t*)response) = strlen((*n)->val)+1+sizeof(int8_t);
				*((int8_t*)(response+sizeof(int32_t))) = 1;
				memcpy(response+sizeof(int32_t)+sizeof(int8_t),(*n)->val,strlen((*n)->val)+1);
				if(bulk_write(client,response,sizeof(int32_t)+sizeof(int8_t)+strlen((*n)->val)+1)<0&&errno!=EPIPE) ERR("write:");
				free(response);
			} else {
				char* response = malloc(sizeof(int32_t)+sizeof(int8_t));
				if (!response) break;
				*((int32_t*)response) = sizeof(int8_t);
				*((int8_t*)(response+sizeof(int32_t))) = 0;
				if(bulk_write(client,response,sizeof(int32_t)+sizeof(int8_t))<0&&errno!=EPIPE) ERR("write:");
				free(response);
			}
			break;
		}
		case 2: // DELETE
		{
			char* key = buffer+sizeof(int8_t);
			int8_t ret = (int8_t)delete(key);
			if (bulk_write(client,(char*)&ret,sizeof(int8_t))<0 &&errno!=EPIPE) ERR("write");
			
			
			if (!ret) printf("Delete successful\n");
			else printf("Delete failed\n");
			break;
		}
	}
	free(buffer);
}

void server_work(int socket){
	fd_set base_rfds, rfds ;
	sigset_t mask, oldmask;
	FD_ZERO(&base_rfds);
	FD_SET(socket, &base_rfds);
	sigemptyset (&mask);
	sigaddset (&mask, SIGINT);
	sigprocmask (SIG_BLOCK, &mask, &oldmask);
	while(work){
		rfds=base_rfds;
		if(pselect(socket+1,&rfds,NULL,NULL,NULL,&oldmask)>0){
			int cfd;
			if((cfd=add_new_client(socket))>=0){
				handle_request(cfd);
				if (TEMP_FAILURE_RETRY(close(cfd))) ERR("close");
			}
		} else {
			if(EINTR==errno) continue;
			ERR("pselect");
		}
	}
	sigprocmask (SIG_UNBLOCK, &mask, NULL);
}

void run_server(char* port){
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("sethandler SIGPIPE");
	if(sethandler(sigint_handler,SIGINT)) ERR("sethandler SIGINT");
	alloc_dict();
	int fd=bind_inet_socket(atoi(port),SOCK_STREAM);
	int new_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_flags);
	printf("Server started. Waiting for connections at port %s.\n",port);
	server_work(fd);
	if(TEMP_FAILURE_RETRY(close(fd))<0) ERR("close");
	dealloc_dict();
	printf("\nServer has terminated.\n");
}
