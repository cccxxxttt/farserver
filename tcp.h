#ifndef __TCP_H
#define __TCP_H

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "farserver.h"

typedef struct {
	struct list_head list;

	char mac[17];

	int routefd;	// route connect

	int pcsrvfd;	// fd from server for pc
	int pcsrvport;	// pc port num
	int tcannel;	// pc_and_server thread cannel flag;

	int pcstat;		// pc connect or disconnect, 0-pcdis, 1-pcen
	int roustat;	// route connect or disconnect, 0-roudis, 1-rouen
}sInfo;


typedef struct {
	int pc_client_fd;	// pc <--> server clients
	sInfo *cl;
}pcInfo;

typedef struct {
	char mac[17];
	int cmdstat;	// 0-disable, 1-enable
}webcInfo;


int deta_pthread_create(pthread_t *thread, void *(*start_routine) (void *), void *arg);

int sock_server(int port);
void *route_and_server(void *arg);
void *web_and_c(void *arg);
void *pc_accept(void *arg);
void *pc_and_server(void *arg);

#endif
