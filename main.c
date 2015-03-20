/********************************************************************\
 *                                                                  *
 * $Id$                                                             *
 * @file main.c                                                     *
 * @brief main loop                                                 *
 * @author Copyright (C) 2015 cxt <xiaotaohuoxiao@163.com>          *
 * @start 2015-2-28                                                 *
 * @end   2015-3-18                                                 *
 *                                                                  *
\********************************************************************/

#include "farserver.h"
#include "tcp.h"

struct list_head clients = LIST_HEAD_INIT(clients);

extern int main_farserver(void);

int main(int argc, char *argv[])
{
	pid_t pid;

	while(1) {
		/* program auto reboot */
		pid = fork();

		if(pid > 0)
			pid = wait(NULL);
		else if(pid == 0) {
			main_farserver();
		}
	}

	return 0;
}

int main_farserver(void)
{
	int routefd, rclient_fd, cfd;
	struct sockaddr_in client_addr;
	unsigned int addrlen = sizeof(struct sockaddr_in);
	char buf[BUFSIZE], time[BUFSIZE];
	int ret;
	sInfo *cl;
	pthread_t tid;
	int is_mac = 0;

	protect_progrem();

	getlocalip();

	/* server: web <--> c */
	cfd = sock_server(C_PORT);
	deta_pthread_create(&tid, web_and_c, (void *)cfd);

	/* route <--> server */
	routefd = sock_server(Route_PORT);
	while(1) {
		if( (rclient_fd = accept(routefd, (struct sockaddr *)&client_addr, &addrlen)) < 0)
			continue;

		//socket_set_keepalive(rclient_fd, 60, 5, 3);		// keep_alive check time: (60+5)*3

		/* read once */
		if((ret = read(rclient_fd, buf, BUFSIZE)) <= 0) {
			close(rclient_fd);
			continue;
		}

		/* 00:16:1c:ff:00:77\r\n */
		if(strlen(buf) != 19)
			continue;
		if(buf[2]!=':' && buf[5]!=':')
			continue;

		is_mac = 0;
		list_for_each_entry(cl, &clients, list) {
			if (strncmp(buf, cl->mac, 17) == 0) {
				is_mac = 1;
				break;
			}
		}

		if(is_mac == 0) {
			/* pthread process */
			cl = (sInfo *)malloc(sizeof(sInfo));
			if(strlen(buf) > 0)
				strncpy(cl->mac, buf, 17);
			cl->mac[17] = '\0';
			cl->routefd = rclient_fd;
			cl->pcsrvfd = -1;
			cl->pcsrvport = -1;
			cl->pcstat = 0;
			cl->roustat = 1;	// route connect
			pthread_mutex_init(&(cl->mutex), NULL);
			list_add_tail(&cl->list, &clients);

			get_local_time(time);
			printf("%s:  route-%d, ip=%s, mac=%s\n", time, cl->routefd, inet_ntoa(client_addr.sin_addr), cl->mac);
			fflush(stdout);
		}
		else {
			if(cl->routefd>0 && cl->routefd!=rclient_fd) {
				close(cl->routefd);
				cl->routefd = rclient_fd;
			}
			cl->roustat = 1;	// route connect
			pthread_mutex_init(&(cl->mutex), NULL);

			get_local_time(time);
			printf("%s:  route-%d, ip=%s, mac=%s\n", time, cl->routefd, inet_ntoa(client_addr.sin_addr), cl->mac);
			fflush(stdout);
		}
	}

	return 0;
}
