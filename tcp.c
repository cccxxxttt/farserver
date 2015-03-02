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

/* change http1.1 long connect, Connection: close */
void modify_connect_close(char urlmsg[])
{
	char *p, *q;
	char tempmsg[TCPSIZE];
	char *src = "Connection: keep-alive";
	char *dst = "Connection: close";

	strcpy(tempmsg, urlmsg);

	p = strstr(tempmsg, src);		// request: keep-alive; repose: Keep-Alive
	if(p != NULL) {
		q = p + strlen(src);
	} else {
		return ;
	}

	memset(urlmsg, '\0', TCPSIZE);
	strncpy(urlmsg, tempmsg, p-tempmsg);
	strcat(urlmsg, dst);
	strcat(urlmsg, q);
}


void modify_http_head(char urlmsg[])
{
	char *p, *q;
	char tempmsg[TCPSIZE];
	char *src = "HTTP/1.1";
	char *dst = "HTTP/1.0";

	strcpy(tempmsg, urlmsg);

	p = strstr(tempmsg, src);		// strstr can find first HTTP/1.1
	if(p != NULL) {
		q = p + strlen(src);
	} else {
		return ;
	}

	memset(urlmsg, '\0', TCPSIZE);
	strncpy(urlmsg, tempmsg, p-tempmsg);
	strcat(urlmsg, dst);
	strcat(urlmsg, q);
}

int read_line(int sockfd, char buf[])
{
	char *temp=buf;
	char c;
	int n;

	while(1)
	{
		n=read(sockfd,&c,1);
		if(n<0)
			return -1;
		else {
			*temp++=c;

			if(c=='\n')
				break;
		}
	}
	*temp = '\0';

	return strlen(buf);
}

// 如果采用短连接，则直接可以通过服务器关闭连接来确定消息的传输长度
ssize_t http_read(int fd, char buf[], size_t count)
{
	int ret, len, i;
	int urlsize;
	char msgsize[BUFSIZE], size[6];
	char temp[BUFSIZE];
	char *p;

	ret = read_line(fd, msgsize);
	if(ret <= 0)
		return -1;

	/* get urlmsg size: UrlSize = xxx\r\n */
	i = 0;
	p = msgsize + strlen("UrlSize = ");
	while(*p != '\r')
		size[i++] = *(p++);
	size[i] = '\0';
	urlsize = atoi(size);

	//printf("urlsize = %d, urlsize=%s, size=%s\n", urlsize, msgsize, size);

	/* read url msg */
	ret = 0;
	if(urlsize > 0) {
		len = urlsize;
		while(len > 0) {
			memset(temp, '\0', BUFSIZE);

			if(len > BUFSIZE)
				ret = read(fd, temp, BUFSIZE);
			else
				ret = read(fd, temp, len);

			len -= ret;

			if(ret > 0)
				strcat(buf, temp);
			else if(ret == 0)
				break;
		}
	}

	return strlen(buf);
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
		printf("\n@@@@@pc--%s is comming... \n", inet_ntoa(client_addr.sin_addr));
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

	memset(urlmsg, '\0', sizeof(urlmsg));
	/* read form pc */
	if((ret = read(pcinfo->pc_client_fd, urlmsg, TCPSIZE)) <= 0) {
		close(pcinfo->pc_client_fd);
		pthread_exit(NULL);
	}
	modify_http_head(urlmsg);
	modify_connect_close(urlmsg);

	printf("pc-%d : %s \n", pcinfo->pc_client_fd, urlmsg);

	pthread_mutex_lock(&(cl->mutex));		// deal one connect until it closed

	/* route connect, have routefd */
	if(cl->roustat==1 && cl->pcstat==1) {
		if(strlen(urlmsg) != 0) {
			/* write to route */
			if((ret = write(cl->routefd, urlmsg, strlen(urlmsg))) < 0){		// send TCPSIZE
				close(pcinfo->pc_client_fd);
				pthread_exit(NULL);
			}

			if(ret == 0) {
				cl->pcstat = 0;
				cl->roustat = 0;
				close(pcinfo->pc_client_fd);
				pthread_exit(NULL);
			}

			/* pc connect, have pcfd */
			memset(urlmsg, '\0', sizeof(urlmsg));
			if(cl->pcstat == 1) {
				/* read form route */
				if((ret = http_read(cl->routefd, urlmsg, TCPSIZE)) < 0){		// read TCPSIZE
					close(pcinfo->pc_client_fd);
					pthread_exit(NULL);
				}

				printf("route read-%d-%d : %s \n", pcinfo->pc_client_fd, ret, urlmsg);
				if(ret == 0) {
					cl->roustat = 0;
					cl->pcstat = 0;
					close(pcinfo->pc_client_fd);
					pthread_exit(NULL);
				}

				/* write to pc */
				if((ret = write(pcinfo->pc_client_fd, urlmsg, strlen(urlmsg))) <= 0) {
					close(pcinfo->pc_client_fd);
					pthread_exit(NULL);
				}
			}
		}
	}

	pcinfo->cl = NULL;
	if(pcinfo->pc_client_fd > 2)
		close(pcinfo->pc_client_fd);
	free(pcinfo);

	pthread_mutex_unlock(&(cl->mutex));

	pthread_exit(NULL);
}

