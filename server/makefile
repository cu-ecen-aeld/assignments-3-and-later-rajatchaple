INCLUDE_DIRS = 
LIB_DIRS = 
ifeq ($(CC),)
CC=$(CROSS_COMPILE)gcc
endif
CDEFS= 
ifeq ($(CFLAGS),)
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS) -Wall -Werror
endif
LIBS= 

ifeq ($(LDFLAGS),)
LDFLAGS= -lpthread -lrt	
endif

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
	$(CC)  $(CFLAGS) -o $@ aesdsocket.o $(LDFLAGS)  $(LIBS)
   

depend:

.c.o:
	$(CC) -MD $(CFLAGS) -c $<
