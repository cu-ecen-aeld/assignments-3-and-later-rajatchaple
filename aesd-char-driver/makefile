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
LDFLAGS= 
endif

PRODUCT=aesd-circular-buffer

HFILES= aesd-circular-buffer.h
CFILES= aesd-circular-buffer.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	${PRODUCT}

default: ${PRODUCT}

clean:
	-rm -f *.o *.NEW *~ *.d
	-rm -f ${PRODUCT} ${GARBAGE}

aesdsocket:	aesdsocket.o
	$(CC)  $(CFLAGS) -o $@ aesd-circular-buffer.o $(LDFLAGS)  $(LIBS)
   

depend:

.c.o:
	$(CC) -MD $(CFLAGS) -c $<