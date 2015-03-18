#
# @bin farserver
# @author cxt <xiaotaohuoxiao@163.com>
# @start 2015-2-28
# @end   2015-3-18
#

CC = gcc
LFAGS = -Wall -o
#LFAGS = -o

SRCS = ${wildcard *.c}
OBJ = farserver

all:
	$(CC) $(LFAGS) $(OBJ) $(SRCS) -lpthread

.PHONY:	clean
clean:
	$(RM) -rf $(OBJ)
