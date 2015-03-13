#ifndef __TCP_H
#define __TCP_H

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <ctype.h>

#include "farserver.h"

//#define DEBUG
#ifdef DEBUG
	#define DEBUG_PRINT(format, ...)	printf(format, ##__VA_ARGS__)
#else
	#define DEBUG_PRINT(format, ...)
#endif

typedef struct {
	struct list_head list;

	char mac[17];

	int routefd;	// route connect

	int pcsrvfd;	// fd from server for pc
	int pcsrvport;	// pc port num
	int tcannel;	// pc_and_server thread cannel flag;

	int pcstat;		// pc connect or disconnect, 0-pcdis, 1-pcen
	int roustat;	// route connect or disconnect, 0-roudis, 1-rouen

	pthread_mutex_t mutex;
}sInfo;


typedef struct {
	int pc_client_fd;	// pc <--> server clients
	sInfo *cl;
}pcInfo;


int deta_pthread_create(pthread_t *thread, void *(*start_routine) (void *), void *arg);

int getlocalip(void);
int sock_server(int port);
void *route_and_server(void *arg);
void *web_and_c(void *arg);
void *pc_accept(void *arg);
void *pc_and_server(void *arg);

#endif
