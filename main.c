#include "farserver.h"
#include "tcp.h"

struct list_head clients = LIST_HEAD_INIT(clients);

int main(void)
{
	int routefd, rclient_fd, cfd;
	struct sockaddr_in client_addr;
	unsigned int addrlen = sizeof(struct sockaddr_in);
	char buf[BUFSIZE];
	int ret;
	sInfo *cl;
	pthread_t tid;

	/* server: web <--> c */
	cfd = sock_server(C_PORT);
	deta_pthread_create(&tid, web_and_c, (void *)cfd);

	/* route <--> server */
	routefd = sock_server(Route_PORT);
	while(1) {
		if( (rclient_fd = accept(routefd, (struct sockaddr *)&client_addr, &addrlen)) < 0)
			continue;

		/* read once */
		if((ret = read(rclient_fd, buf, BUFSIZE)) <= 0) {
			close(rclient_fd);
			continue;
		}

		/* pthread process */
		cl = (sInfo *)malloc(sizeof(sInfo));
		if(strlen(buf) == 17)
			strcpy(cl->mac, buf);
		cl->routefd = rclient_fd;
		cl->roustat = 1;	// route connect
		list_add_tail(&cl->list, &clients);
	}

	return 0;
}
