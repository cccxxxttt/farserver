/********************************************************************\
 *                                                                  *
 * $Id$                                                             *
 * @file tcp.c                                                      *
 * @brief tcp functions                                             *
 * @author Copyright (C) 2015 cxt <xiaotaohuoxiao@163.com>          *
 * @start 2015-2-28                                                 *
 * @end   2015-3-18                                                 *
 *                                                                  *
\********************************************************************/

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

/* analyze data, eg: 11:22:33:44:55:66;0; (mac;en;) */
int phpcdata(char mac[], char *en, char buf[])
{
	char *p;
	int i = 0;

	while(buf[i] != '\0') {
		buf[i] = tolower(buf[i]);
		i++;
	}

	/* mac */
	p = strtok(buf, ";");
	strcpy(mac, p);

	/* en */
	p = strtok(NULL, ";");
	*en = *p;

	return 0;
}

/* create radom port(10000 ~ 20000) */
int arrayNum[10000]={-1};
int port_create()
{
	int pcsrv_port = -1;
	int i, num = 10000;

	for(i=0; i<num; i++) {
		if(arrayNum[i] == -1) {
			arrayNum[i] = i + 10000;
			break;
		}
	}

	if(i < 10000)
		pcsrv_port = arrayNum[i];

	return pcsrv_port;
}

/* report to php */
int port_report(int webfd, int port, int en)
{
	char buf[BUFSIZE];
	int ret;

	memset(buf, '\0', sizeof(buf));
	switch(en) {
		case 0:
			sprintf(buf, "0;");		//   has break!!!!!!!!
			break;

		case 1:
			sprintf(buf, "1;%s:%d;", SRV_DOMAIN, port);		//  route connect!!!!!!	(1;rz.wifisz.com:10000;)
			break;

		case 2:
			sprintf(buf, "2;");		//  route has no connect!!!!!!
			break;

		default:
			sprintf(buf, "%d;", en);
			break;
	}

	//printf("en=%d, buf=%s\n", en, buf);

	if((ret = write(webfd, buf, BUFSIZE)) <= 0)
		return ret;

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

	DEBUG_PRINT("local_ip = %s\n", SRV_IP);

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
	int ret;

	while(1)
	{
		ret = read(sockfd,&c,1);
		if(ret <= 0)
			return ret;
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
ssize_t http_read(int fd, char *buf)
{
	int ret, i;
	int urlsize = 0;
	char msgsize[BUFSIZE], size[6];
	char temp[BUFSIZE];
	char *p;
	int retsize = 0;

	ret = read_line(fd, msgsize);
	if(ret <= 0)
		return -1;

	/* get urlmsg size: UrlSize = xxx\r\n */
	if(strncmp(msgsize, "UrlSize", 7) != 0)
		return -1;

	i = 0;
	p = msgsize + strlen("UrlSize = ");
	while(*p != '\r')
		size[i++] = *(p++);
	size[i] = '\0';
	urlsize = atoi(size);

	//DEBUG_PRINT("urlsize = %d, urlsize=%s, size=%s\n", urlsize, msgsize, size);

	/* read url msg */
	while(urlsize > 0) {
		memset(temp, '\0', BUFSIZE);

		if(urlsize > BUFSIZE)
			ret = read(fd, temp, BUFSIZE);
		else
			ret = read(fd, temp, urlsize);

		if(ret > 0) {
			// don't use strcat and strncpy, because sometimes jgp etc msg has '\0'
			memcpy(buf+retsize, temp, ret);
		}
		else
			return ret;

		urlsize -= ret;
		retsize += ret;
	}

	return retsize;
}

void get_local_time(char time_buf[])
{
	time_t timep;
	struct tm *tm;

	time(&timep);
	tm = localtime(&timep);

	sprintf(time_buf, "%d/%d/%d-%d:%d:%d",
		(1900+tm->tm_year), (1+tm->tm_mon), tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/* socket set keepalive, timeout can set tcp stat, write() and read() return 0 */
int socket_set_keepalive(int fd, int idle, int intv, int cnt)
{
	int alive;

	/* Set: use keepalive on fd */
	alive = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &alive, sizeof alive) != 0)
		return -1;

	/* idle秒钟无数据，触发保活机制，发送保活包 */
	if(setsockopt (fd, SOL_TCP, TCP_KEEPIDLE, &idle, sizeof idle) != 0)
		return -1;

	/* 如果没有收到回应，则intv秒钟后重发保活包 */
	if (setsockopt (fd, SOL_TCP, TCP_KEEPINTVL, &intv, sizeof intv) != 0)
		return -1;

	/* 连续cnt次没收到保活包，视为连接失效 */
	if (setsockopt (fd, SOL_TCP, TCP_KEEPCNT, &cnt, sizeof cnt) != 0)
		return -1;

	return 1;
}

/* when one socket close, write twice takes a SIGPIPE, exit progrem */
void protect_progrem(void)
{
	signal(SIGPIPE, SIG_IGN);
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

	/* arrayNum[] init */
	int i;
	for(i=0; i<10000; i++)
		arrayNum[i] = -1;

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

		DEBUG_PRINT("macmsg=%s, en=%c\n", macmsg, en);

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

					if(pcsrv_port >= 10000) {
						pcsrv_fd = sock_server(pcsrv_port);
						cl->pcsrvfd = pcsrv_fd;
						cl->pcsrvport = pcsrv_port;

						/* pc <--> server */
						deta_pthread_create(&tid, pc_accept, cl);
						cl->tcannel = 0;
					}
				}

				/* report port to php */
				port_report(web_fd, cl->pcsrvport, 1);
			}
			/* uncontrol */
			else if(en == '0'){
				cl->tcannel = 1;		// cannel pc_and_server thread
				cl->pcstat = 0;
				if(cl->pcsrvfd != 0) { close(cl->pcsrvfd);  cl->pcsrvfd=-1; }
				if(cl->pcsrvport >= 10000)
					arrayNum[cl->pcsrvport - 10000] = -1;
				port_report(web_fd, cl->pcsrvport, 0);
				cl->pcsrvport = -1;
			}
			else {
				port_report(web_fd, cl->pcsrvport, en);
			}
		}
		else {
			port_report(web_fd, cl->pcsrvport, 2);
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
	int ret;
	fd_set fds;
	struct timeval timeout = {3, 0};

	cl->pcstat= 1;			// pc connect

	while(1) {
		if(cl->tcannel == 1)  		// cannel thread
			break;

		FD_ZERO(&fds);
		FD_SET(cl->pcsrvfd, &fds);
		ret = select(cl->pcsrvfd+1, &fds, NULL, NULL, &timeout);
		if(ret <= 0)
			continue;
		else {
			if( (pcfd = accept(cl->pcsrvfd, (struct sockaddr *)&client_addr, &addrlen)) < 0)
				continue;

			//DEBUG_PRINT("\npc--%s is comming... \n", inet_ntoa(client_addr.sin_addr));

			if(cl->roustat == 0) {
				char buf[1024];
				char *repose = "<html><body><h1>route has not connect!!!!</h1></body></html>";
				sprintf(buf,
					"HTTP/1.0 200 OK\r\n"
					"Connection: close\r\n"
					"Content-Type: text/html\r\n"
					"Content-Length: %d\r\n"
					"\r\n"
					"%s",

					strlen(repose),
					repose
					);

				write(pcfd, buf, strlen(buf));
				close(pcfd);
				continue;
			}

			/* pc <--> server */
			pcInfo *pcinfo = (pcInfo *)malloc(sizeof(pcInfo));
			pcinfo->pc_client_fd= pcfd;
			pcinfo->cl = cl;

			deta_pthread_create(&tid, pc_and_server, pcinfo);
		}
	}

	close(cl->pcsrvfd);
	cl->pcsrvfd=-1;
	if(cl->pcsrvport >= 10000)
		arrayNum[cl->pcsrvport - 10000] = -1;
	cl->pcsrvport = -1;
	cl->pcstat = 0;
	DEBUG_PRINT("pc_accept exit!!!\n");

	pthread_exit(NULL);
}


ssize_t http_write(int fd, char buf[], size_t count)
{
	int ret;
	char temp[TCPSIZE];

	/* send count first */
	memset(temp, '\0', TCPSIZE);
	sprintf(temp, "UrlSize = %d\r\n", count);
	ret = write(fd, temp, strlen(temp));
	if(ret <= 0)
		return ret;

	//DEBUG_PRINT("urlsize = %s\n", temp);

	/* send reponse */
	ret = write(fd, buf, count);
	if(ret <= 0)
		return ret;

	return ret;
}


ssize_t pc_read_head(int pcfd, char urlmsg[], unsigned long *textLength)
{
	int ret, i;
	char method[5], lineBuf[BUFSIZE];
	char *p;
	char size[20];
	int retsize = 0;

	/* read first line */
	ret = read_line(pcfd, method);
	if(ret <= 0)
		return ret;

	strcat(urlmsg, method);
	retsize += ret;

	/* head */
	while(1) {
		ret = read_line(pcfd, lineBuf);
		if(ret <= 0)
			return ret;
		else
			strcat(urlmsg, lineBuf);

		retsize += ret;

		if(strcmp(lineBuf, "\r\n") == 0)		// head end
			break;
	}

	/* GET: has head msg, no data msg */
	/* POST: has head msg and data msg */
	if(strncmp(method, "POST", 4) == 0) {
		*textLength = 0;

		p = strstr(urlmsg, "Content-Length:");
		if(p != NULL) {
			p = p + 15;
			while(*p++ != ' ');

			i = 0;
			while(*p != '\r')
				size[i++] = *p++;
			size[i] = '\0';
			*textLength = atoi(size);
		}
	}

	return retsize;
}

ssize_t pc_read_send_data(int pcfd, int routefd, unsigned long textLength)
{
	int ret;
	char textBuf[TCPSIZE];

	/* data */
	if(textLength > 0) {
		while(textLength > 0) {
			/* read from pc */
			memset(textBuf, '\0', TCPSIZE);
			if(textLength > TCPSIZE)
				ret = read(pcfd, textBuf, TCPSIZE);
			else
				ret = read(pcfd, textBuf, textLength);

			textLength -= ret;

			if(ret <= 0)
				return -1;
			DEBUG_PRINT("aaaaaaaaaaaaa textLength=%ld, ret=%d, len=%d\n", textLength, ret, strlen(textBuf));

			/* debug, read fireware write to cxt.bin, compile result is ok */
            #if 0
			FILE *fp;
			fp = fopen("./cxt.bin", "ab");
			fwrite(textBuf, ret, 1, fp);
			fclose(fp);
			#endif

			/* write to route */
			if((ret = http_write(routefd, textBuf, ret)) <= 0)
				return ret;
		}
	}

	return 0;
}

int route_to_pc(int pcfd, int routefd)
{
	int ret;
	char urlmsg[TCPSIZE];
	int retsize = 0;
	int pc_err_flag = 0;

	while(1) {
		/* read form route */
		memset(urlmsg, '\0', sizeof(urlmsg));
		if((ret = http_read(routefd, urlmsg)) <= 0)
			return RU_MSG_ERR;

		DEBUG_PRINT("route-%d-%d-%d\n", pcfd, ret, strlen(urlmsg));

		if(strcmp(urlmsg, "end\r\n") == 0)
			break;

		retsize += ret;

		//DEBUG_PRINT("route read-%d-%d : %s \n", pcfd, ret, urlmsg);
		/* write to pc */
		if(pc_err_flag == 0)
			if((ret = write(pcfd, urlmsg, ret)) <= 0)
				pc_err_flag = 1;
	}

	if(pc_err_flag == 1)
		return PC_MSG_ERR;

	DEBUG_PRINT("route-end-%d\n", retsize);
	return retsize;
}

void *pc_and_server(void *arg)
{
	pcInfo *pcinfo = (pcInfo *)arg;
	char urlmsg[TCPSIZE];
	int ret;
	unsigned long textLength;
	sInfo *cl = pcinfo->cl;

	#ifdef DEBUG
	int pc_fd = pcinfo->pc_client_fd;
	#endif
	DEBUG_PRINT("\n@@ pc--%d is comming... \n", pc_fd);

	/* read head form pc */
	textLength = 0;
	memset(urlmsg, '\0', sizeof(urlmsg));
	if((ret = pc_read_head(pcinfo->pc_client_fd, urlmsg, &textLength)) <= 0) {
		pcinfo->cl = NULL;
		if(pcinfo->pc_client_fd > 2)
			close(pcinfo->pc_client_fd);
		free(pcinfo);
		DEBUG_PRINT("\n@@2pc--%d is end... \n", pc_fd);
		pthread_exit(NULL);
	}

	pthread_mutex_lock(&(cl->mutex));		// deal one connect until it closed

	do {
		if(cl->roustat==1 && (cl->pcstat==1 || pcinfo->pc_client_fd > 2)) {
			DEBUG_PRINT("\n########################\n");

			modify_http_head(urlmsg);
			modify_connect_close(urlmsg);

			//DEBUG_PRINT("\ncom~~~ pcfd=%d, pcstat=%d\n", pcinfo->pc_client_fd, cl->pcstat);
			DEBUG_PRINT("pc-%d : %s", pcinfo->pc_client_fd, urlmsg);

		#if 1
			/* send start msg(use write), use to tcp keep alive, see farclient */
			if((ret = write(cl->routefd, "start\r\n", strlen("start\r\n"))) <= 0) {
				cl->pcstat = 0;
				cl->roustat = 0;
				break;
			}
		#endif

			/* write head to route (zhu: urlmsg size has change, don't use ret) */
			if((ret = http_write(cl->routefd, urlmsg, strlen(urlmsg))) <= 0) {
				cl->pcstat = 0;
				cl->roustat = 0;
				break;
			}

			//DEBUG_PRINT("@@@@@@textLength = %d\n", textLength);

			/* POST data send */
			if(textLength > 0) {
				if((ret = pc_read_send_data(pcinfo->pc_client_fd, cl->routefd, textLength)) < 0) {
					break;
				}
			}

			/* send end msg */
			if((ret = http_write(cl->routefd, "end\r\n", strlen("end\r\n"))) <= 0) {
				cl->pcstat = 0;
				cl->roustat = 0;
				break;
			}

			DEBUG_PRINT("pc-%d-end\n", pcinfo->pc_client_fd);

			/* route to pc */
			ret = route_to_pc(pcinfo->pc_client_fd, cl->routefd);
			if(ret == PC_MSG_ERR) {
				cl->pcstat = 0;
				break;
			}
			else if(ret == RU_MSG_ERR) {
				cl->roustat = 0;
				cl->pcstat = 0;
				break;
			}

		}
	}while(0);

	pthread_mutex_unlock(&(cl->mutex));

	pcinfo->cl = NULL;
	if(pcinfo->pc_client_fd > 2)
		close(pcinfo->pc_client_fd);
	free(pcinfo);

	DEBUG_PRINT("\n@@pc--%d is end... \n", pc_fd);

	pthread_exit(NULL);
}

