#include "tcp.h"

extern struct list_head clients;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


int deta_pthread_create(pthread_t *thread, void *(*start_routine) (void *), void *arg)
{
	pthread_attr_t attr;
	pthread_attr_init (&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(pthread_create (thread, &attr, start_routine, arg) < 0) {
		return -1;
	}

	return 0;
}

/* analyze data, eg: 11:22:33:44:55:66;0; (mac;en;) */
int phpcdata(char mac[], char en, char buf[])
{
	char *p;

	/* mac */
	p = strtok(buf, ";");
	strcpy(mac, p);

	/* en */
	p = strtok(NULL, ";");
	en = *p;

	return 0;
}

/* create radom port(10000 ~ 20000) */
int arrayNum[10000]={0};
int port_create()
{
	int pcsrv_port = 0;
	int i, num = 20000;

	for(i=10000; i<num; i++) {
		if(arrayNum[i-10000] == 0) {
			arrayNum[i-10000] = i;
			break;
		}
	}

	pcsrv_port = i;

	return pcsrv_port;
}

/* report to php */
int port_report(int webfd, int port)
{
	char buf[BUFSIZE];
	int ret;

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s:%d\r\n", SRV_DOMAIN, port);

	if((ret = write(webfd, buf, BUFSIZE)) <= 0)
		return -1;

	return 0;
}

int sock_server(int port)
{
	int sock_fd = -1;

	/* socket */
	if( (sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	int on = 1;
	if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		return -1;

	/* bind */
	struct sockaddr_in sin;
    bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(SRV_IP);
	if(bind(sock_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		return -1;

	/* listen */
	if(listen(sock_fd, LISTEN_NUM) < 0)
		return -1;

	return sock_fd;
}

void *web_and_c(void *arg)
{
	int cfd = (int)arg;
	struct sockaddr_in client_addr;
	unsigned int addrlen = sizeof(struct sockaddr_in);
	int web_fd, ret, pcsrv_port, pcsrv_fd;
	char buf[BUFSIZE];
	char macmsg[18], en;
	sInfo *cl;
	pthread_t tid;
	int is_mac = 0;

	while(1) {
		if( (web_fd = accept(cfd, (struct sockaddr *)&client_addr, &addrlen)) < 0)
			continue;

		if((ret = read(web_fd, buf, BUFSIZE)) <= 0) {
			close(web_fd);
			continue;
		}

		/* data analyze */
		memset(macmsg, '\0', sizeof(macmsg));
		en = '0';
		phpcdata(macmsg, en, buf);

		is_mac = 0;
		list_for_each_entry(cl, &clients, list) {
			if (strncmp(macmsg, cl->mac, 17) == 0) {
				is_mac = 1;
				break;
			}
		}

		/* find the route */
		if((is_mac==1) && (cl->roustat==1)) {

			/* control */
			if(en == '1') {
				/* create port */
				pcsrv_port = port_create();

				pcsrv_fd = sock_server(pcsrv_port);
				cl->pcsrvfd = pcsrv_fd;
				cl->pcsrvport = pcsrv_port;

				/* pc <--> server */
				deta_pthread_create(&tid, pc_accept, cl);
				cl->tcannel_1 = 0;
				cl->tcannel_2 = 0;

				/* report port to php */
				port_report(web_fd, pcsrv_port);
			}
			/* uncontrol */
			else {
				cl->tcannel_2 = 1;		// cannel pc_and_server thread
				cl->pcstat = 0;
				if(cl->pcsrvfd != 0) { close(cl->pcsrvfd);  cl->pcsrvfd=0; }
				arrayNum[cl->pcsrvport] = 0;
			}
		}

		close(web_fd);

	}

	pthread_exit(NULL);
}

void *pc_accept(void *arg)
{
	sInfo *cl = (sInfo *)arg;
	struct sockaddr_in client_addr;
	unsigned int addrlen = sizeof(struct sockaddr_in);
	int pcfd;
	pthread_t tid;

	cl->pcstat= 1;	// route connect

	while(1) {
		if(cl->tcannel_1 == 1)		// cannel thread
			break;

		if(cl->roustat == 0)
			break;

		if( (pcfd = accept(cl->pcsrvfd, (struct sockaddr *)&client_addr, &addrlen)) < 0)
			continue;

		/* pc <--> server have some clients */
		pcInfo *pcinfo = (pcInfo *)malloc(sizeof(pcInfo));
		pcinfo->pc_client_fd= pcfd;
		pcinfo->cl = cl;

		deta_pthread_create(&tid, pc_and_server, pcinfo);
	}

	pthread_exit(NULL);
}

void *pc_and_server(void *arg)
{
	pcInfo *pcinfo = (pcInfo *)arg;
	char urlmsg[TCPSIZE];
	int ret;
	sInfo *cl = pcinfo->cl;

	while(1) {
		if(cl->tcannel_2 == 1) {		// cannel thread
			cl->tcannel_1 = 1;
			close(pcinfo->pc_client_fd);
			free(pcinfo);
			break;
		}

		/* route connect, have routefd */
		if(cl->roustat==1 && cl->pcstat==1) {
			/* read form pc */
			if((ret = read(pcinfo->pc_client_fd, urlmsg, TCPSIZE)) < 0)
				continue;

			if(ret == 0) {
				cl->pcstat = 0;
				continue;
			}

			pthread_mutex_lock(&mutex);		// only allow one write, and read once

			/* write to route */
			if((ret = write(cl->routefd, urlmsg, TCPSIZE)) < 0)
				continue;

			if(ret == 0) {
				cl->pcstat = 0;
				cl->roustat = 0;
				break;
			}

			/* pc connect, have pcfd */
			if(cl->pcstat == 1) {
				/* read form route */
				if((ret = read(cl->routefd, urlmsg, TCPSIZE)) < 0)
					continue;

				if(ret == 0) {
					cl->roustat = 0;
					cl->pcstat = 0;
					continue;
				}

				/* write to pc */
				if((ret = write(pcinfo->pc_client_fd, urlmsg, TCPSIZE)) < 0) {
				}
				if(ret == 0)
					cl->pcstat = 0;
			}

			pthread_mutex_unlock(&mutex);
		}
	}

	pthread_exit(NULL);
}

