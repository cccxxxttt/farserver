#include "tcp.h"

extern struct list_head clients;

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

ssize_t tcp_read(int fd, void *buf, size_t count)
{
	int ret, len;
	char tmp[BUFSIZE];

	len = count;
	while(len > 0) {
		memset(tmp, '\0', sizeof(tmp));

		if(len > BUFSIZE) {
			if((ret = read(fd, tmp, BUFSIZE)) < 0)
				continue;
		}
		else {
			if((ret = read(fd, tmp, len)) < 0)
				continue;
		}

		len -= ret;
		strcat(buf, tmp);
	}

	return ret;
}

/* analyze data, eg: 11:22:33:44:55:66;0; (mac;en;) */
int phpcdata(char mac[], char *en, char buf[])
{
	char *p;

	/* mac */
	p = strtok(buf, ";");
	strcpy(mac, p);

	/* en */
	p = strtok(NULL, ";");
	*en = *p;

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
int port_report(int webfd, int port, int en)
{
	char buf[BUFSIZE];
	char *con = " ";
	int ret;

	if(en == 0)
		con = "  has break!!!!!!!!";

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%s:%d%s\r\n", SRV_DOMAIN, port, con);

	if((ret = write(webfd, buf, BUFSIZE)) <= 0)
		return -1;

	return 0;
}

char SRV_IP[17];
int getlocalip(void)
{
    int sfd, intr;
    struct ifreq buf[16];
    struct ifconf ifc;
	char *ip;

    sfd = socket (AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0)
        return -1;

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t)buf;
    if (ioctl(sfd, SIOCGIFCONF, (char *)&ifc))
        return -1;

    intr = ifc.ifc_len / sizeof(struct ifreq);
    while (intr-- > 0 && ioctl(sfd, SIOCGIFADDR, (char *)&buf[intr]));

    close(sfd);

    ip = inet_ntoa(((struct sockaddr_in*)(&buf[intr].ifr_addr))->sin_addr);
	strcpy(SRV_IP, ip);

	printf("local_ip = %s\n", SRV_IP);

	return 0;
}


/* analyse response url data, judge connect */
int response_close(char urlmsg[])
{
	char *p = NULL, *q = NULL;
	int ret = 0, i;
	char buf[BUFSIZE];

	p = strstr(urlmsg, "Connection:");
	if(p != NULL) {
		/* get line --  Connection: .....*/
		q = p;
		i = 0;
		while(*(q) != '\n') {
			buf[i++] = *q;
			q++;
		}
		buf[i] = '\0';

		p = strstr(buf, "close");
		if(p != NULL)
			ret = CONCLOSE;
	}

	return ret;
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
		phpcdata(macmsg, &en, buf);

		printf("macmsg=%s, en=%c\n", macmsg, en);

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
				if(cl->pcsrvport == -1) {
					/* create port */
					pcsrv_port = port_create();

					pcsrv_fd = sock_server(pcsrv_port);
					cl->pcsrvfd = pcsrv_fd;
					cl->pcsrvport = pcsrv_port;

					/* pc <--> server */
					deta_pthread_create(&tid, pc_accept, cl);
					cl->tcannel = 0;
				}

				/* report port to php */
				port_report(web_fd, cl->pcsrvport, 1);
			}
			/* uncontrol */
			else {
				cl->tcannel = 1;		// cannel pc_and_server thread
				cl->pcstat = 0;
				if(cl->pcsrvfd != 0) { close(cl->pcsrvfd);  cl->pcsrvfd=-1; }
				arrayNum[cl->pcsrvport - 10000] = 0;
				port_report(web_fd, cl->pcsrvport, 0);
				cl->pcsrvport = -1;
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

	cl->pcstat= 1;			// pc connect

	while(1) {
		if(cl->tcannel == 1) {		// cannel thread
			break;
		}

		if(cl->roustat == 0)
			break;

		if( (pcfd = accept(cl->pcsrvfd, (struct sockaddr *)&client_addr, &addrlen)) < 0)
			continue;

		printf("\npc--%s is comming... \n", inet_ntoa(client_addr.sin_addr));

		/* pc <--> server */
		pcInfo *pcinfo = (pcInfo *)malloc(sizeof(pcInfo));
		pcinfo->pc_client_fd= pcfd;
		pcinfo->cl = cl;

		deta_pthread_create(&tid, pc_and_server, pcinfo);
	}

	close(pcfd);

	pthread_exit(NULL);
}

void *pc_and_server(void *arg)
{
	pcInfo *pcinfo = (pcInfo *)arg;
	char urlmsg[TCPSIZE];
	int ret;
	sInfo *cl = pcinfo->cl;
	printf("com~~~ pcfd=%d, pcstat=%d\n", pcinfo->pc_client_fd, cl->pcstat);

	while(1) {
		/* route connect, have routefd */
		memset(urlmsg, '\0', sizeof(urlmsg));
		if(cl->roustat==1 && cl->pcstat==1) {
			/* read form pc */
			if((ret = read(pcinfo->pc_client_fd, urlmsg, TCPSIZE)) <= 0) {
				close(pcinfo->pc_client_fd);
				pthread_exit(NULL);
			}
			printf("pc : %s \n", urlmsg);
			pthread_mutex_lock(&(cl->mutex));		// only allow one write, and read once


			if(urlmsg != NULL) {
				/* write to route */
				if((ret = write(cl->routefd, urlmsg, strlen(urlmsg))) < 0){		// send TCPSIZE
					close(pcinfo->pc_client_fd);
					break;
				}

				if(ret == 0) {
					cl->pcstat = 0;
					cl->roustat = 0;
					close(pcinfo->pc_client_fd);
					break;
				}

				/* pc connect, have pcfd */
				memset(urlmsg, '\0', sizeof(urlmsg));
				if(cl->pcstat == 1) {
					/* read form route */
					if((ret = read(cl->routefd, urlmsg, TCPSIZE)) < 0){		// read TCPSIZE
						close(pcinfo->pc_client_fd);
						break;
					}

					printf("route read : %s \n", urlmsg);
					if(ret == 0) {
						cl->roustat = 0;
						cl->pcstat = 0;
						close(pcinfo->pc_client_fd);
						break;
					}
					//printf("read from route ok!!!!\n");

					/* write to pc */
					if((ret = write(pcinfo->pc_client_fd, urlmsg, strlen(urlmsg))) <= 0) {
						close(pcinfo->pc_client_fd);
						break;
					}

					/* response say connect: close */
					if(response_close(urlmsg) == CONCLOSE) {
						close(pcinfo->pc_client_fd);
						break;
					}
				}
			}
		}

		pthread_mutex_unlock(&(cl->mutex));
	}

	free(pcinfo);

	pthread_exit(NULL);
}

