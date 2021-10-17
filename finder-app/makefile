INCLUDE_DIRS = 
LIB_DIRS = 
CC=$(CROSS_COMPILE)gcc

CDEFS= 
CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)
LDFLAGS= 
LIBS= 

PRODUCT=writer

HFILES=
CFILES= writer.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

.DEFAULT_GOAL := all

all: ${PRODUCT}


writer:	writer.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ writer.o $(LIBS)
   

.c.o:
	$(CC) -MD $(CFLAGS) -c $<

cleans:
	-rm -f *.o *.NEW *~ *.d
	-rm -f ${PRODUCT} ${GARBAGE}
