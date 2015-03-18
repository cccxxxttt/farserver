/********************************************************************\
 *                                                                  *
 * $Id$                                                             *
 * @file farserver.h                                                *
 * @brief server params                                             *
 * @author Copyright (C) 2015 cxt <xiaotaohuoxiao@163.com>          *
 * @start 2015-2-28                                                 *
 * @end   2015-3-18                                                 *
 *                                                                  *
\********************************************************************/

#ifndef __FARSERVER_H
#define __FARSERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "kernel_list.h"

#define SRV_DOMAIN	"rz.wifisz.com"
#define Route_PORT	9999
#define C_PORT		52360
#define LISTEN_NUM 	1024
#define BUFSIZE 	1024
#define TCPSIZE		65535
#define CONCLOSE 	100


#endif
