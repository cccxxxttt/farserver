
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
