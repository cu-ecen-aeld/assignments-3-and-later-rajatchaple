INCLUDE_DIRS = 
LIB_DIRS = 
CC=$(CROSS_COMPILE)gcc

CDEFS= 
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS) -Wall -Werror
LDFLAGS= 
LIBS= -lpthread -lrt	

PRODUCT=aesdsocket

HFILES= queue.h
CFILES= aesdsocket.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	${PRODUCT}

default: ${PRODUCT}

clean:
	-rm -f *.o *.NEW *~ *.d
	-rm -f ${PRODUCT} ${GARBAGE}

aesdsocket:	aesdsocket.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ aesdsocket.o $(LIBS)
   

depend:

.c.o:
	$(CC) -MD $(CFLAGS) -c $<
