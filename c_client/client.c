#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#define ERR(source) (perror(source),\
                fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                exit(EXIT_FAILURE))

int sethandler( void (*f)(int), int sigNo) {
        struct sigaction act;
        memset(&act, 0, sizeof(struct sigaction));
        act.sa_handler = f;
        if (-1==sigaction(sigNo, &act, NULL))
                return -1;
        return 0;
}
int make_socket(void){
        int sock;
        sock = socket(PF_INET,SOCK_STREAM,0);
        if(sock < 0) ERR("socket");
        return sock;
}
struct sockaddr_in make_address(char *address, char *port){
        int ret;
        struct sockaddr_in addr;
        struct addrinfo *result;
        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        if((ret=getaddrinfo(address,port, &hints, &result))){
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
                exit(EXIT_FAILURE);
        }
        addr = *(struct sockaddr_in *)(result->ai_addr);
        freeaddrinfo(result);
        return addr;
}
int connect_socket(char *name, char *port){
        struct sockaddr_in addr;
        int socketfd;
        socketfd = make_socket();
        addr=make_address(name,port);
        if(connect(socketfd,(struct sockaddr*) &addr,sizeof(struct sockaddr_in)) < 0){
                if(errno!=EINTR) ERR("connect");
                else {
                        fd_set wfds ;
                        int status;
                        socklen_t size = sizeof(int);
                        FD_ZERO(&wfds);
                        FD_SET(socketfd, &wfds);
                        if(TEMP_FAILURE_RETRY(select(socketfd+1,NULL,&wfds,NULL,NULL))<0) ERR("select");
                        if(getsockopt(socketfd,SOL_SOCKET,SO_ERROR,&status,&size)<0) ERR("getsockopt");
                        if(0!=status) ERR("connect");
                }
        }
        return socketfd;
}
ssize_t bulk_read(int fd, char *buf, size_t count){
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
ssize_t bulk_write(int fd, char *buf, size_t count){
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

void usage(char * name){
        fprintf(stderr,"USAGE: %s domain port type{insert=0,search=1,delete=2} key [value]\n",name);
        exit(EXIT_FAILURE);
}

void insert_request(char** argv, int fd){
        char* key = argv[4];
        char* val = argv[5];
        int32_t val_offset = sizeof(int8_t)+sizeof(int32_t) + strlen(key)+1;
        int32_t length = (int32_t)(sizeof(int8_t)+sizeof(int32_t)+strlen(key)+1+strlen(val)+1);
        char* buffer = malloc(length+sizeof(int32_t));
        if (!buffer) ERR("malloc");
        *((int32_t*)(buffer)) = length;
        *((int8_t*)(buffer+sizeof(int32_t))) = 0;
        *((int32_t*)(buffer+sizeof(int32_t)+sizeof(int8_t))) = val_offset;
        memcpy((char*)(buffer+2*sizeof(int32_t)+sizeof(int8_t)),key,strlen(key)+1);
        memcpy((char*)(buffer+2*sizeof(int32_t)+sizeof(int8_t)+strlen(key)+1),val,strlen(val)+1);
        if(bulk_write(fd,buffer,length+sizeof(int32_t))<0&&errno!=EPIPE) ERR("write:");
        free(buffer);
        
        int8_t status;
        if((bulk_read(fd,(char*)&status,sizeof(int8_t)))<sizeof(int8_t)) return;
        printf("Status: %d\n",status);
}

char* search_request(char** argv, int fd){
	char* key = argv[4];
	char* buffer = malloc(sizeof(int8_t)+sizeof(int32_t)+strlen(key)+1);
	if (!buffer) ERR("malloc");
	*((int32_t*)buffer) = (int32_t)(sizeof(int8_t)+strlen(key)+1);
	*((int8_t*)(buffer+sizeof(int32_t))) = 1;
	memcpy(buffer+sizeof(int32_t)+sizeof(int8_t),key,strlen(key)+1);
	if(bulk_write(fd,buffer,sizeof(int32_t)+sizeof(int8_t)+strlen(key)+1)<0&&errno!=EPIPE) ERR("write:");
	free(buffer);
	int32_t length;
	ssize_t size;
	if((size=bulk_read(fd,(char*)&length,sizeof(int32_t)))<sizeof(int32_t)) return NULL;
	buffer = malloc(length);
	if (!buffer) return NULL;
	if((size=bulk_read(fd,buffer,length))!=length) return NULL;
	int8_t status;
	status = *((int8_t*)buffer);
	if (status){
		char* value = malloc(length-sizeof(int8_t));
		if (!value) ERR("malloc");
		memcpy(value,buffer+sizeof(int8_t),length-sizeof(int8_t));
		free(buffer);
		return value;
	}
	else{
		printf("Not found.\n");
		free(buffer);
		return NULL;	
	}
}

void delete_request(char** argv, int fd){
	char* key = argv[4];
	char* buffer = malloc(sizeof(int8_t)+sizeof(int32_t)+strlen(key)+1);
	if (!buffer) ERR("malloc");
	*((int32_t*)buffer) = (int32_t)(sizeof(int8_t)+strlen(key)+1);
	*((int8_t*)(buffer+sizeof(int32_t))) = 2;
	memcpy(buffer+sizeof(int32_t)+sizeof(int8_t),key,strlen(key)+1);
	if(bulk_write(fd,buffer,sizeof(int32_t)+sizeof(int32_t)+strlen(key)+1)<0&&errno!=EPIPE) ERR("write:");
	free(buffer);
	int8_t status;
	if((bulk_read(fd,(char*)&status,sizeof(int8_t)))<sizeof(int8_t)) return;
	printf("Status: %d\n",status);
}

int main(int argc, char** argv) {
		if (argc<5) usage(argv[0]);
		int8_t type = atoi(argv[3]);
		if (type==0 && argc!=6) usage(argv[0]);
        int fd;
        if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
        
        fd=connect_socket(argv[1],argv[2]);
        switch(type){
			case 0:
			{
				insert_request(argv,fd);
				break;
			}
			case 1:
			{
				char* resp = search_request(argv,fd);
				if (resp!=NULL){
					printf("RECEIVED: %s\n",resp);
					free(resp);
				}
				break;
			}
			case 2:
			{
				delete_request(argv,fd);
				break;
			}
		}
        
        if(TEMP_FAILURE_RETRY(close(fd))<0)ERR("close");
        return EXIT_SUCCESS;
}
