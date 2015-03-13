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
	char buf[BUFSIZE];
	int ret;
	sInfo *cl;
	pthread_t tid;
	int is_mac = 0;

	getlocalip();

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

		is_mac = 0;
		list_for_each_entry(cl, &clients, list) {
			if (strncmp(buf, cl->mac, 15) == 0) {
				is_mac = 1;
				break;
			}
		}

		if(is_mac == 0) {
			/* pthread process */
			cl = (sInfo *)malloc(sizeof(sInfo));
			if(strlen(buf) > 0)
				strcpy(cl->mac, buf);
			cl->routefd = rclient_fd;
			cl->pcsrvfd = -1;
			cl->pcsrvport = -1;
			cl->pcstat = 0;
			cl->roustat = 1;	// route connect
			pthread_mutex_init(&(cl->mutex), NULL);
			list_add_tail(&cl->list, &clients);

			DEBUG_PRINT("\nroute--%s is comming...   mac=%s\n", inet_ntoa(client_addr.sin_addr), cl->mac);
		}
		else {
			cl->routefd = rclient_fd;
			cl->roustat = 1;	// route connect
			pthread_mutex_init(&(cl->mutex), NULL);

			DEBUG_PRINT("\nroute--%s is comming...   mac=%s\n", inet_ntoa(client_addr.sin_addr), cl->mac);
		}
	}

	return 0;
}
