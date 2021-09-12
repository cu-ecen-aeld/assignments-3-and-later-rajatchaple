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

all:	${PRODUCT}

clean:
	-rm -f *.o *.NEW *~ *.d
	-rm -f ${PRODUCT} ${GARBAGE}

writer:	writer.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ writer.o $(LIBS)
   

depend:

.c.o:
	$(CC) -MD $(CFLAGS) -c $<
